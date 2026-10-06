#pragma once
// test_data.hpp — the manifest's own pinned files, read once, for the range-model and gate tests (SPEC-measmod.md §8).

#include <catch2/catch_test_macros.hpp>

#include <odl/eop/series.hpp>
#include <odl/io/crd.hpp>
#include <odl/io/sinex.hpp>
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
