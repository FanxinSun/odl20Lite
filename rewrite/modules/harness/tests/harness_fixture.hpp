#pragma once
// harness_fixture.hpp — the pinned inputs of the harness tests: paths are compile definitions of odl_harness_tests ONLY (never of the library).
#include <catch2/catch_test_macros.hpp>

#include <odl/harness/gnss_campaign.hpp>
#include <odl/harness/igs_metadata.hpp>
#include <odl/harness/statistics.hpp>

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace hfx {

inline std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

inline const odl::harness::GnssWorld& world() {
    static const std::unique_ptr<odl::harness::GnssWorld> w = [] {
        odl::harness::GnssWorldPaths p;
        p.leap_seconds = ODL_H_LEAP_SECOND_FILE;
        p.de440s = ODL_H_DE440S_BSP;
        p.gm = ODL_H_GM_DE440_TPC;
        p.egm2008 = ODL_H_EGM2008_COEFFICIENTS;
        p.fes2004 = ODL_H_FES2004_CNM_SNM;
        p.c04 = ODL_H_C04_FILE;
        p.cache_root = ODL_H_MANIFEST_CACHE_ROOT;
        auto r = odl::harness::GnssWorld::load(p);
        if (!r) FAIL("GnssWorld::load: " << std::string(r.error().id) << " " << r.error().message);
        return std::move(*r);
    }();
    return *w;
}

}  // namespace hfx
