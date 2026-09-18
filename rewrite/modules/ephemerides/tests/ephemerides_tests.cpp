// ephemerides_tests.cpp — SPEC-ephemerides.md §8.
//
// The gate is JPL's own published verification set. `testpo.440` carries ~13 200
// cases of the form
//
//     de#  date  jed  target  centre  coordinate  value
//
// with values in AU and AU/day, in the CLASSIC body numbering. Reproducing them
// pins the interpolation, the units and the body mapping at once.

#include <odl/ephemerides/ephemeris.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using Catch::Matchers::WithinAbs;
using namespace odl::eph;
using odl::time::Epoch;
using odl::time::LeapTable;
using odl::time::TimeScale;

namespace {

const LeapTable& leaps() {
    static const LeapTable t = [] {
        std::ifstream f(ODL_LEAP_SECOND_FILE, std::ios::binary);
        REQUIRE(f.good());
        std::ostringstream ss; ss << f.rdbuf();
        auto r = LeapTable::parse(ss.str(), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

struct TestpoCase {
    double jed;
    int target, centre, coord;
    double value;
};

const std::vector<TestpoCase>& testpo() {
    static const std::vector<TestpoCase> cases = [] {
        std::vector<TestpoCase> out;
        std::ifstream f(ODL_TESTPO_440);
        REQUIRE(f.good());
        std::string line;
        bool started = false;
        while (std::getline(f, line)) {
            if (!started) { if (line.find("EOT") != std::string::npos) started = true; continue; }
            std::istringstream in(line);
            int de = 0; std::string date;
            TestpoCase c{};
            if (!(in >> de >> date >> c.jed >> c.target >> c.centre >> c.coord >> c.value)) continue;
            out.push_back(c);
        }
        return out;
    }();
    return cases;
}

Epoch tdb_at(double jd) {
    auto e = Epoch::from_two_part_jd(TimeScale::TDB, std::floor(jd), jd - std::floor(jd), leaps());
    if (!e.has_value()) FAIL("epoch: " << e.error().message);
    return *e;
}

/// Run every testpo case the kernel covers. Returns (checked, skipped, worst).
/// The two reasons a case is not checked are different facts and are counted
/// separately: "not a body" is a property of testpo (targets 14-17 are
/// nutations, librations and time-scale differences), "outside coverage" is a
/// property of the kernel. Lumping them would hide which kernel was used.
struct Sweep {
    long checked = 0, not_a_body = 0, outside_coverage = 0;
    double worst_au = 0.0;
    std::string worst_what;
};

Sweep run_sweep(const Ephemeris& eph, double au_km) {
    Sweep s;
    for (const TestpoCase& c : testpo()) {
        Body target{}, centre{};
        // Targets 14-17 are nutations, librations and time-scale differences
        // rather than bodies; EPH-A-007 covers 16 separately.
        if (!body_of_classic_id(c.target, target) || !body_of_classic_id(c.centre, centre)) {
            ++s.not_a_body; continue;
        }
        const auto st = eph.state(target, centre, tdb_at(c.jed), leaps());
        if (!st.has_value()) { ++s.outside_coverage; continue; }

        // The module returns km and km/s (EPH-R-013). testpo is in AU and
        // AU/day, so the comparison goes through the AU READ FROM THE KERNEL —
        // which is asserted exactly by EPH-A-003 and perturbed by EPH-A-004, so
        // it cannot cancel a units error.
        const odl::Vec3& r = st->position();
        const odl::Vec3& v = st->velocity();
        const double got =
            c.coord == 1 ? r.x / au_km
          : c.coord == 2 ? r.y / au_km
          : c.coord == 3 ? r.z / au_km
          : c.coord == 4 ? v.x * 86400.0 / au_km
          : c.coord == 5 ? v.y * 86400.0 / au_km
          :                v.z * 86400.0 / au_km;
        const double err = std::abs(got - c.value);
        if (err > s.worst_au) {
            s.worst_au = err;
            s.worst_what = std::string(name_of(target)) + " wrt " + std::string(name_of(centre)) +
                           " coord " + std::to_string(c.coord) + " at JD " + std::to_string(c.jed);
        }
        ++s.checked;
    }
    return s;
}

Ephemeris open_or_fail(const char* path, const char* id) {
    auto e = Ephemeris::open({path}, {EphProvenance{id, path, ""}});
    if (!e.has_value()) FAIL(id << ": " << e.error().message);
    return std::move(*e);
}

}  // namespace

TEST_CASE("EPH-A-003: the astronomical unit is the IAU 2012 defining value",
          "[eph][spec][gate]") {
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const auto au = eph.astronomical_unit();
    INFO("AU = " << au.km << " km, from_kernel = " << au.from_kernel);
    REQUIRE_THAT(au.km, WithinAbs(Ephemeris::kAstronomicalUnitKm, 1e-6));

    // An SPK carries NO constants — measured here rather than assumed, and the
    // reason spec v1.2 amends EPH-R-012: "read the AU from the kernel" is not
    // satisfiable on the SPK route the plan mandates.
    REQUIRE_FALSE(au.from_kernel);
    REQUIRE_FALSE(eph.constant("AU").has_value());
    REQUIRE_FALSE(eph.constant("EMRAT").has_value());
}

TEST_CASE("EPH-A-001/002: THE GATE — JPL's published testpo.440, short kernel",
          "[eph][spec][gate][published]") {
    REQUIRE(testpo().size() > 13000);
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const double au = eph.astronomical_unit().km;
    const Sweep s = run_sweep(eph, au);

    // EPH-A-001 reports its denominator: "the gate passed" without a count is
    // the unstated-denominator error in another disguise.
    INFO("de440s: " << s.checked << " of " << testpo().size() << " checked; "
         << s.not_a_body << " not a body (testpo targets 14-17); "
         << s.outside_coverage << " outside the short kernel's 1849-2150 span");
    INFO("worst residual " << s.worst_au << " AU = " << s.worst_au * au * 1000.0 << " m, at "
         << s.worst_what);
    // Exact, because both inputs are hash-pinned: a change in either should be
    // visible rather than absorbed by an inequality.
    REQUIRE(s.checked == 3099);
    REQUIRE(s.not_a_body == 1847);
    REQUIRE(s.outside_coverage == 8255);
    REQUIRE(s.worst_au < 1e-13);
}

TEST_CASE("EPH-A-001: THE FULL SWEEP — the long kernel, which must actually RUN",
          "[eph][spec][gate][published][slow]") {
    // EPH-Q-003 was ruled that pinning both kernels is not enough: the full
    // sweep must be run at this step's gate and its case count recorded, because
    // "pinned both, CI uses the short one" decays into the full coverage being
    // notional within two layers.
    const Ephemeris eph = open_or_fail(ODL_DE440_BSP, "de440-spk");
    const double au = eph.astronomical_unit().km;
    const Sweep s = run_sweep(eph, au);
    INFO("de440 (full): " << s.checked << " of " << testpo().size() << " checked; "
         << s.not_a_body << " not a body; " << s.outside_coverage << " outside coverage");
    INFO("worst residual " << s.worst_au << " AU = " << s.worst_au * au * 1000.0 << " m, at "
         << s.worst_what);
    // EVERY body case in the published set, with nothing skipped for coverage.
    REQUIRE(s.checked == 11354);
    REQUIRE(s.not_a_body == 1847);
    REQUIRE(s.outside_coverage == 0);
    REQUIRE(s.checked + s.not_a_body == static_cast<long>(testpo().size()));
    REQUIRE(s.worst_au < 1e-13);
}

TEST_CASE("EPH-A-004: the units check cannot pass against a wrong AU", "[eph][spec][gate]") {
    // Without this, EPH-A-002's km-to-AU comparison could pass against a
    // constant that cancelled a units error. Perturbing the AU by one part in a
    // million must break it.
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const double au = eph.astronomical_unit().km;
    const Sweep good = run_sweep(eph, au);
    const Sweep bad = run_sweep(eph, au * (1.0 + 1e-6));
    INFO("worst with the true AU " << good.worst_au << ", with AU*(1+1e-6) " << bad.worst_au);
    REQUIRE(good.worst_au < 1e-13);
    REQUIRE(bad.worst_au > 1e-9);          // fails by orders of magnitude, as it must
}

TEST_CASE("EPH-A-005: Earth and the Earth-Moon barycentre are 4600-4700 km apart",
          "[eph][spec]") {
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const auto st = eph.state(Body::Earth, Body::EarthMoonBarycentre, tdb_at(2458849.5), leaps());
    REQUIRE(st.has_value());
    const double d = st->position().norm();
    INFO("Earth - EMB separation " << d << " km");
    REQUIRE(d > 4000.0);
    REQUIRE(d < 5200.0);
    // EPH-R-022: they are distinct members and cannot be confused.
    STATIC_REQUIRE(Body::Earth != Body::EarthMoonBarycentre);
}

TEST_CASE("EPH-A-007: TDB-TT from the kernel agrees with SPEC-time, in that sense",
          "[eph][spec]") {
    // EPH-R-004. The kernel's own body 16 is TT-TDB; this tree returns TDB-TT.
    // A sign error would show as these two disagreeing by EXACTLY TWICE the
    // value, which reads like a factor-of-two bug rather than a naming one.
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    double worst_ns = 0.0;
    int n = 0;
    for (int day = 0; day < 365; day += 7) {
        const Epoch e = tdb_at(2458849.5 + day);
        const auto from_kernel = eph.tdb_minus_tt(e, leaps());
        if (!from_kernel.has_value()) { INFO(from_kernel.error().message); break; }
        const double from_erfa = e.tdb_minus_tt().to_seconds();
        worst_ns = std::max(worst_ns, std::abs(from_kernel->to_seconds() - from_erfa) * 1e9);
        // The sign must match, not merely the magnitude.
        REQUIRE(from_kernel->to_seconds() * from_erfa > 0.0);
        ++n;
    }
    if (n == 0) {
        WARN("de440s carries no TT-TDB record; EPH-A-007 needs a *t kernel (see the report)");
    } else {
        INFO(n << " epochs; worst difference " << worst_ns << " ns");
        REQUIRE(worst_ns < 100.0);
    }
}

TEST_CASE("EPH-A-009/014: refusals", "[eph][refusal]") {
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    SECTION("EPH-F-002: outside the coverage of that body in that kernel") {
        const auto cov = eph.coverage(Body::Sun);
        REQUIRE(cov.has_value());
        const auto r = eph.state(Body::Sun, Body::SolarSystemBarycentre,
                                 tdb_at(cov->last_jd_tdb + 100.0), leaps());
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "EPH-F-002");
        REQUIRE(r.error().message.find("no extrapolation") != std::string::npos);
        REQUIRE(r.error().message.find("Sun") != std::string::npos);
    }
    SECTION("EPH-F-003: a kernel path that does not exist") {
        const auto r = Ephemeris::open({"/nonexistent/de440.bsp"}, {});
        REQUIRE_FALSE(r.has_value());
        REQUIRE(r.error().id == "EPH-F-003");
        REQUIRE(r.error().message.find("EPH-R-040") != std::string::npos);
    }
}

TEST_CASE("EPH-A-012: two Ephemeris instances coexist", "[eph][spec]") {
    const Ephemeris a = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const Ephemeris b = open_or_fail(ODL_DE440_BSP, "de440-spk");
    for (int i = 0; i < 3; ++i) {
        const auto ca = a.coverage(Body::Sun);
        const auto cb = b.coverage(Body::Sun);
        REQUIRE(ca.has_value());
        REQUIRE(cb.has_value());
        REQUIRE(cb->first_jd_tdb < ca->first_jd_tdb);   // the full kernel starts earlier
    }
}
