#pragma once
// test_data.hpp — the manifest's own pinned files, read once, for the range-model and gate tests (SPEC-measmod.md §8).

#include <catch2/catch_test_macros.hpp>

#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/io/crd.hpp>
#include <odl/io/horizons.hpp>
#include <odl/io/sinex.hpp>
#include <odl/io/sp3.hpp>
#include <odl/measmod/range.hpp>
#include <odl/measmod/registry.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace odl::measmod::testing {

inline std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

inline const odl::time::LeapTable& leaps() {
    static const odl::time::LeapTable t = [] {
        auto r = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

inline odl::time::Epoch utc(int y, int mo, int d, int h, int mi, double s) {
    auto e = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC, odl::time::Calendar{y, mo, d, h, mi, s}, leaps());
    REQUIRE(e.has_value());
    return *e;
}

inline const odl::eop::EopSeries& c04() {
    static const odl::eop::EopSeries s = [] {
        auto r = odl::eop::EopSeries::load_c04(slurp(ODL_C04_FILE), odl::eop::EopProvenance{"eop-c04-20", "", "", ""}, leaps());
        if (!r) FAIL("C04: " << r.error().message);
        return *r;
    }();
    return s;
}

/// finals2000A.all as pinned (observed to 2026-09-17, predicted beyond): the Earth orientation of the 2026-09 epochs of the vendored Horizons table, with the predictions allowed.
inline const odl::eop::EopSeries& finals() {
    static const odl::eop::EopSeries s = [] {
        auto r = odl::eop::EopSeries::load_finals2000a(slurp(ODL_FINALS2000A_FILE), odl::eop::EopProvenance{"eop-finals2000a", "", "", ""}, leaps());
        if (!r) FAIL("finals2000A: " << r.error().message);
        return *r;
    }();
    return s;
}
inline odl::eop::EopPolicy predictions_allowed() {
    odl::eop::EopPolicy policy;
    policy.max_quality = odl::eop::Quality::Predicted;
    return policy;
}

/// The vendored Horizons vector table of ACS3 (5 real records at 30-minute steps; ICRF, geocentre, TDB, geometric).
inline const odl::io::HorizonsEphemeris& acs3_table() {
    static const odl::io::HorizonsEphemeris t = [] {
        auto r = odl::io::read_horizons(slurp(ODL_ACS3_HORIZONS_FILE));
        if (!r) FAIL("the vendored Horizons table did not read: " << r.error().id << " " << r.error().message);
        return *r;
    }();
    return t;
}

/// The real ILRS weekly SP3 of LAGEOS-1 (the week ending 2026-01-03; SP3-c, UTC, SLR20, 5040 epochs). Its header and first record are read by the position tests;
/// NO residual against it is computed anywhere before the envelope of MEAS-A-101 is committed.
inline const odl::io::Sp3File& real_sp3() {
    static const odl::io::Sp3File f = [] {
        auto r = odl::io::read_sp3(slurp(ODL_ILRS_SP3_FILE));
        if (!r) FAIL("the real ILRS SP3 did not read: " << r.error().id << " " << r.error().message);
        return *r;
    }();
    return f;
}

/// The planetary ephemeris de440s as the manifest pins it (the Earth's barycentric motion of the Astrometric angle gate, MEAS-A-096, -099).
inline const odl::eph::Ephemeris& ephemeris_de440s() {
    static const auto e = odl::eph::Ephemeris::open({ODL_DE440S_BSP}, {});
    if (!e) FAIL("de440s did not open: " << e.error().id << " " << e.error().message);
    return *e;
}

inline const SlrRegistry& real_registry() {
    static const SlrRegistry r = [] {
        auto a = odl::io::read_sinex(slurp(ODL_SLRF2020_FILE));
        REQUIRE(a.has_value());
        auto b = odl::io::read_sinex(slurp(ODL_SLR_ECC_FILE));
        REQUIRE(b.has_value());
        auto reg = SlrRegistry::build(*a, *b, slurp(ODL_SLR_PSD_FILE), leaps(), RegistrySources{ODL_SLRF2020_SHA256, ODL_SLR_ECC_SHA256, ODL_SLR_PSD_SHA256});
        if (!reg) FAIL("the real registry did not build: " << reg.error().id << " " << reg.error().message);
        return *reg;
    }();
    return r;
}

/// The real January 2026 LAGEOS-1 file as passes (807). The first is Yarragadee's, 2026-01-01 02:07:49.
inline const std::vector<odl::io::CrdPass>& real_passes() {
    static const std::vector<odl::io::CrdPass> p = [] {
        auto r = odl::io::read_crd_passes(slurp(ODL_LAGEOS1_NP_FILE));
        if (!r) FAIL("the real normal-point file did not read as passes: " << r.error().id << " " << r.error().message);
        return *r;
    }();
    return p;
}

/// LAGEOS-1's ILRS satellite identifier (CRD H3 field 3) and the centre-of-mass correction used as a TEST INPUT: 251 mm, the value the ILRS publishes
/// for LAGEOS (the citation says where; the model treats it as a cited input, MEAS-R-032).
inline constexpr long kLageos1Id = 7603901;
inline SphericalCentreOfMass lageos1_com(double metres = 0.251) {
    auto c = SphericalCentreOfMass::make(kLageos1Id, metres, "ILRS LAGEOS centre-of-mass correction (a test input; SPEC-measmod MEAS-R-032)");
    REQUIRE(c.has_value());
    return *c;
}

}  // namespace odl::measmod::testing
