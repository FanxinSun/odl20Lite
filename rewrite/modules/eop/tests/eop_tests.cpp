// eop_tests.cpp — SPEC-eop.md §8.
//
// The strongest rows here are the four PUBLISHED TEST CASES carried in the IERS
// reference routines' own headers.  EOP-R-008: a coefficient table transcribed
// from the Conventions is verified against them, and a transcription that fails
// its published test case is a build failure rather than a warning.

#include "tides.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using namespace odl::eop;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToMicroArcsec = 180.0 * 3600.0 * 1.0e6 / kPi;
}  // namespace

TEST_CASE("EOP-A-003: ORTHO_EOP's published test case, within the disagreement the "
          "Conventions themselves state", "[eop][spec][published]") {
    const auto a = tides::arguments_at_mjd(47100.0);
    const tides::Correction c = tides::ocean_tides(a);

    const double dx_uas = c.dxp * kRadToMicroArcsec;
    const double dy_uas = c.dyp * kRadToMicroArcsec;
    const double dut1_us = c.dut1 * 1.0e6;

    INFO("dx  = " << dx_uas  << " uas, published -162.8386373279636530");
    INFO("dy  = " << dy_uas  << " uas, published  117.7907525842668974");
    INFO("dUT1= " << dut1_us << " us, published  -23.39092370609808214");

    // TN36 §8.2 is explicit about why this is not exact: the tables "cannot be
    // found in the code of ORTHO_EOP.F", the IERS implemented them separately in
    // interp.f, and "the two routines agree at the level of A FEW
    // MICROARCSECONDS in polar motion and A FEW TENTHS OF A MICROSECOND in UT1".
    // This tree implements the tables (EOP-R-007), so that published bound is the
    // right tolerance. SPEC-eop's "to the last published digit" was unachievable
    // by the route the same spec mandates — see the report.
    REQUIRE_THAT(dx_uas,  WithinAbs(-162.8386373279636530, 1.0));
    REQUIRE_THAT(dy_uas,  WithinAbs( 117.7907525842668974, 1.0));
    REQUIRE_THAT(dut1_us, WithinAbs( -23.39092370609808214, 0.05));
}

TEST_CASE("EOP-A-004: PMSDNUT2's published test case", "[eop][spec][published]") {
    // MJD 54335 (2007-08-23).  Table 5.1a is implemented directly rather than
    // re-derived, so this one is expected to agree closely.
    const auto a = tides::arguments_at_mjd(54335.0);
    const tides::Correction c = tides::libration(a);
    const double dx_uas = c.dxp * kRadToMicroArcsec;
    const double dy_uas = c.dyp * kRadToMicroArcsec;
    INFO("dx = " << dx_uas << " uas, published 24.83144238273364834");
    INFO("dy = " << dy_uas << " uas, published -14.09240692041837661");
    REQUIRE_THAT(dx_uas, WithinAbs( 24.83144238273364834, 0.1));
    REQUIRE_THAT(dy_uas, WithinAbs(-14.09240692041837661, 0.1));
}

TEST_CASE("EOP-A-005/006: UTLIBR's two published test cases", "[eop][spec][published]") {
    struct Case { double mjd; double dut1_us; double dlod_us_per_day; };
    const Case cases[] = {
        {44239.1,  2.441143834386761746, -14.78971247349449492},
        {55227.4, -2.655705844335680244,  27.39445826599846967},
    };
    for (const auto& k : cases) {
        const auto a = tides::arguments_at_mjd(k.mjd);
        const tides::Correction c = tides::libration(a);
        INFO("MJD " << k.mjd << ": dUT1 = " << c.dut1 * 1e6 << " us, published " << k.dut1_us);
        INFO("MJD " << k.mjd << ": dLOD = " << c.dlod * 1e6 << " us/day, published "
                    << k.dlod_us_per_day);
        REQUIRE_THAT(c.dut1 * 1e6, WithinAbs(k.dut1_us, 0.02));
        REQUIRE_THAT(c.dlod * 1e6, WithinAbs(k.dlod_us_per_day, 0.1));
    }
}

TEST_CASE("the tidal corrections are of the magnitude SPEC-eop §4.4 predicts",
          "[eop][spec]") {
    // The spec justifies including each of the four corrections by the satellite
    // position it is worth at 7000 km.  Those magnitudes are checked here so
    // that a transcription error large enough to matter shows up even if it
    // somehow passed a single published epoch.
    double max_pole_uas = 0.0, max_ut1_us = 0.0, max_lib_uas = 0.0, max_libut1_us = 0.0;
    for (int i = 0; i < 2000; ++i) {
        const double mjd = 55000.0 + static_cast<double>(i) * 0.037;
        const auto a = tides::arguments_at_mjd(mjd);
        const auto o = tides::ocean_tides(a);
        const auto l = tides::libration(a);
        max_pole_uas  = std::max(max_pole_uas, std::abs(o.dxp) * kRadToMicroArcsec);
        max_ut1_us    = std::max(max_ut1_us, std::abs(o.dut1) * 1e6);
        max_lib_uas   = std::max(max_lib_uas, std::abs(l.dxp) * kRadToMicroArcsec);
        max_libut1_us = std::max(max_libut1_us, std::abs(l.dut1) * 1e6);
    }
    INFO("ocean pole " << max_pole_uas << " uas, ocean UT1 " << max_ut1_us
         << " us, libration pole " << max_lib_uas << " uas, libration UT1 "
         << max_libut1_us << " us");
    REQUIRE(max_pole_uas  > 100.0);   REQUIRE(max_pole_uas  < 1500.0);
    REQUIRE(max_ut1_us    > 10.0);    REQUIRE(max_ut1_us    < 100.0);
    REQUIRE(max_lib_uas   > 5.0);     REQUIRE(max_lib_uas   < 100.0);
    REQUIRE(max_libut1_us > 0.5);     REQUIRE(max_libut1_us < 20.0);
}

// --------------------------------------------------------------------------- //
// The series: parsing, interpolation, splice and coverage, against the real
// hash-pinned IERS products.

#include <odl/eop/series.hpp>

#include <fstream>
#include <sstream>

namespace {

std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}

const odl::time::LeapTable& leaps() {
    static const odl::time::LeapTable t = [] {
        auto r = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE),
                                             {"iers-leap-seconds", "", "Bulletin C 72"});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

const EopSeries& c04() {
    static const EopSeries s = [] {
        auto r = EopSeries::load_c04(slurp(ODL_C04_FILE),
                                     EopProvenance{"eop-c04-20", "", "", ""}, leaps());
        if (!r.has_value()) FAIL("C04 load refused: " << r.error().message);
        return *r;
    }();
    return s;
}

const EopSeries& finals() {
    static const EopSeries s = [] {
        auto r = EopSeries::load_finals2000a(slurp(ODL_FINALS_FILE),
                                             EopProvenance{"eop-finals2000a", "", "", ""}, leaps());
        if (!r.has_value()) FAIL("finals2000A load refused: " << r.error().message);
        return *r;
    }();
    return s;
}

odl::time::Epoch at_mjd(double mjd) {
    auto e = odl::time::Epoch::from_two_part_jd(odl::time::TimeScale::UTC,
                                                std::floor(mjd) + 2400000.5,
                                                mjd - std::floor(mjd), leaps());
    if (!e.has_value()) FAIL("epoch: " << e.error().message);
    return *e;
}

EopPolicy planning() {
    EopPolicy p; p.max_quality = Quality::Predicted; return p;
}

}  // namespace

TEST_CASE("EOP-A-001/002: the C04 series parses, and the pre-1972 policy is applied",
          "[eop][spec]") {
    const auto& s = c04();
    INFO("rows " << s.rows().size() << ", skipped pre-1972 " << s.rows_before_1972());
    // The file starts 1962-01-01, but SPEC-time refuses UTC before 1972, so the
    // usable series starts at MJD 41317 and the excluded rows are counted rather
    // than silently dropped.
    // 1962-01-01 (MJD 37665) to 1971-12-31 (MJD 41316) inclusive is 3652 days.
    REQUIRE(s.rows_before_1972() == 3652);
    REQUIRE(s.rows().front().mjd == 41317);
    REQUIRE(s.rows().size() > 19000);
    for (std::size_t i = 1; i < s.rows().size(); ++i) {
        REQUIRE(s.rows()[i].mjd == s.rows()[i - 1].mjd + 1);
    }
    // EOP-R-023: the file's own model line, recorded as found and not reconciled.
    INFO("header model: \"" << s.provenance().front().header_model << "\"");
    REQUIRE(s.provenance().front().header_model.find("IAU 2000") != std::string::npos);
}

TEST_CASE("EOP-A-007: interpolation reproduces a tabulated node exactly", "[eop][spec]") {
    const auto& s = c04();
    const auto& row = s.rows()[5000];
    const auto got = s.raw_at(at_mjd(static_cast<double>(row.mjd)), planning());
    REQUIRE(got.has_value());
    REQUIRE_THAT(got->xp, WithinAbs(row.value.xp, 1e-18));
    REQUIRE_THAT(got->yp, WithinAbs(row.value.yp, 1e-18));
    REQUIRE_THAT(got->dut1, WithinAbs(row.value.dut1, 1e-12));
    REQUIRE_FALSE(got->subdaily_applied);
}

TEST_CASE("EOP-A-008: at() minus raw_at() is exactly the four sub-daily corrections",
          "[eop][spec]") {
    const auto& s = c04();
    const double mjd = 58000.37;
    const auto full = s.at(at_mjd(mjd), planning());
    const auto raw  = s.raw_at(at_mjd(mjd), planning());
    REQUIRE(full.has_value());
    REQUIRE(raw.has_value());
    REQUIRE(full->subdaily_applied);
    REQUIRE_FALSE(raw->subdaily_applied);

    const auto a = tides::arguments_at_mjd(mjd);
    const auto c = tides::subdaily(a);
    REQUIRE_THAT(full->xp - raw->xp, WithinAbs(c.dxp, 1e-16));
    REQUIRE_THAT(full->dut1 - raw->dut1, WithinAbs(c.dut1, 1e-12));
    INFO("sub-daily at MJD " << mjd << ": pole " << c.dxp * kRadToMicroArcsec
         << " uas, UT1 " << c.dut1 * 1e6 << " us");
}

TEST_CASE("EOP-A-010: the two products agree where they overlap", "[eop][spec]") {
    // This is the row that catches the mas/arcsec and ms/s traps of SPEC-eop §3.5:
    // a units error in either parser is a factor of a thousand and cannot pass.
    const auto& a = c04();
    const auto& b = finals();
    const std::int64_t lo = std::max(a.coverage().first_mjd, b.coverage().first_mjd);
    const std::int64_t hi = std::min(a.coverage().last_mjd, b.coverage().last_mjd);
    REQUIRE(hi > lo + 200);

    double worst_pole_mas = 0.0, worst_ut1_us = 0.0;
    int compared = 0;
    for (std::int64_t mjd = hi - 200; mjd < hi; ++mjd) {
        const auto ra = a.raw_at(at_mjd(static_cast<double>(mjd)), planning());
        const auto rb = b.raw_at(at_mjd(static_cast<double>(mjd)), planning());
        if (!ra.has_value() || !rb.has_value()) continue;
        worst_pole_mas = std::max(worst_pole_mas,
                                  std::abs(ra->xp - rb->xp) * kRadToMicroArcsec * 1e-3);
        worst_ut1_us = std::max(worst_ut1_us, std::abs(ra->dut1 - rb->dut1) * 1e6);
        ++compared;
    }
    INFO(compared << " days compared; worst pole " << worst_pole_mas << " mas, worst UT1 "
         << worst_ut1_us << " us");
    REQUIRE(compared > 100);
    REQUIRE(worst_pole_mas < 0.5);
    REQUIRE(worst_ut1_us < 50.0);
}

TEST_CASE("EOP-A-015/016/032: coverage is answerable first, and the margin is enforced",
          "[eop][refusal]") {
    const auto& s = c04();
    const Coverage cov = s.coverage();
    // EOP-R-052/053: the reported interval is already two days inside the data,
    // and it can be asked for before any epoch is requested.
    REQUIRE(cov.first_mjd == s.rows().front().mjd + 1);
    REQUIRE(cov.last_mjd == s.rows().back().mjd - 2);

    REQUIRE(s.at(at_mjd(static_cast<double>(cov.last_mjd)), planning()).has_value());

    for (double mjd : {static_cast<double>(cov.last_mjd + 1),
                       static_cast<double>(cov.last_mjd + 30),
                       static_cast<double>(cov.first_mjd - 1)}) {
        const auto r = s.at(at_mjd(mjd), planning());
        INFO("requested MJD " << mjd);
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "EOP-F-007");
        REQUIRE(r.error().message.find("no extrapolation") != std::string::npos);
        REQUIRE(r.error().message.find(std::to_string(cov.last_mjd)) != std::string::npos);
    }
}

TEST_CASE("EOP-A-017: predicted values are refused under a campaign policy",
          "[eop][refusal]") {
    const auto& s = finals();
    EopPolicy campaign;              // default: Rapid is the worst accepted
    campaign.max_quality = Quality::Rapid;
    const Coverage cov = s.coverage();

    // finals2000A runs a year into the future; the far end is predicted.
    const auto far = s.at(at_mjd(static_cast<double>(cov.last_mjd)), campaign);
    REQUIRE_FALSE(far.has_value());
    REQUIRE(far.error().id == "EOP-F-005");
    REQUIRE(far.error().message.find("PREDICTED") != std::string::npos);
    // The same epoch under a planning policy is allowed.
    REQUIRE(s.at(at_mjd(static_cast<double>(cov.last_mjd)), planning()).has_value());
}

TEST_CASE("EOP-A-024/025: the wrong product is refused", "[eop][refusal]") {
    const auto bad14 = std::string(
        "# EOP (IERS) 14 C04 TIME SERIES  consistent with ITRF 2014\n"
        "1972   1   1   0  41317.00  0.1 0.1 0.01 0.0 0.0 0.0 0.0 0.001\n");
    const auto r = EopSeries::load_c04(bad14, EopProvenance{"wrong", "", "", ""}, leaps());
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().id == "EOP-F-001");
    REQUIRE(r.error().message.find("ITRF2020") != std::string::npos);

    // finals.all carries dPsi/dEps against IAU 1980 in the columns finals2000A
    // uses for dX/dY.  dPsi runs to tens of arcseconds, i.e. tens of thousands of
    // mas, so the range check catches the substitution even though the layouts
    // are identical.
    std::string finals_1980(190, ' ');
    finals_1980.replace(0, 16, "72 1 1 41317.00 ");
    finals_1980.replace(16, 1, "I");
    finals_1980.replace(18, 9, " 0.100000");
    finals_1980.replace(37, 9, " 0.200000");
    finals_1980.replace(57, 1, "I");
    finals_1980.replace(58, 10, " 0.0100000");
    finals_1980.replace(95, 1, "I");
    finals_1980.replace(97, 9, "-52195.00");   // dPsi in mas: finals.all, not finals2000A
    finals_1980.replace(116, 9, " -3875.00");
    const auto r2 = EopSeries::load_finals2000a(finals_1980 + "\n",
                                                EopProvenance{"finals.all", "", "", ""}, leaps());
    REQUIRE_FALSE(r2.has_value());
    REQUIRE(r2.error().id == "EOP-F-004");
    REQUIRE(r2.error().message.find("EOP-R-002") != std::string::npos);
}

TEST_CASE("EOP-A-011: the UT1 range limit is configurable, not hard-coded at 0.9 s",
          "[eop][spec]") {
    // CGPM Resolution 4 (2022) raises the permitted maximum when leap seconds
    // stop, so a hard-coded 0.9 s would make this module fail on data it will be
    // given.  The policy field exists and is honoured in both directions.
    EopPolicy strict = planning(); strict.dut1_range_limit_s = 1.0e-6;
    const auto r = c04().at(at_mjd(58000.0), strict);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().id == "EOP-F-004");
    REQUIRE(r.error().message.find("CGPM") != std::string::npos);

    EopPolicy loose = planning(); loose.dut1_range_limit_s = 5.0;
    REQUIRE(c04().at(at_mjd(58000.0), loose).has_value());
}

TEST_CASE("EOP-A-020: two series coexist; there is no global EOP state", "[eop][spec]") {
    const auto& a = c04();
    const auto& b = finals();
    for (int i = 0; i < 3; ++i) {
        const auto ra = a.at(at_mjd(58000.25), planning());
        const auto rb = b.at(at_mjd(58000.25), planning());
        REQUIRE(ra.has_value());
        REQUIRE(rb.has_value());
        REQUIRE(a.provenance().front().product == "EOP 20 C04");
        REQUIRE(b.provenance().front().product == "finals2000A");
    }
}
