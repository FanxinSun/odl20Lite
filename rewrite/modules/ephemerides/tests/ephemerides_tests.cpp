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
#include <type_traits>
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
        const auto st = eph.relative_state(target, centre, tdb_at(c.jed), leaps());
        if (!st.has_value()) { ++s.outside_coverage; continue; }

        // The module returns km and km/s (EPH-R-013). testpo is in AU and
        // AU/day, so the comparison goes through the AU READ FROM THE KERNEL —
        // which is asserted exactly by EPH-A-003 and perturbed by EPH-A-004, so
        // it cannot cancel a units error.
        const odl::Vec3& r = st->position_km;
        const odl::Vec3& v = st->velocity_km_s;
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
    const Ephemeris eph = open_or_fail(ODL_DE440T_BSP, "de440t-spk");
    const double au = eph.astronomical_unit().km;
    const Sweep s = run_sweep(eph, au);
    INFO("de440t (full): " << s.checked << " of " << testpo().size() << " checked; "
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
    const auto st = eph.relative_state(Body::Earth, Body::EarthMoonBarycentre, tdb_at(2458849.5), leaps());
    REQUIRE(st.has_value());
    const double d = st->position_km.norm();
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
    // de440t, NOT de440s.  This test could not run at all until L2 step 3
    // substituted de440t.bsp for de440.bsp: the short kernel carries no TT-TDB
    // record, so `n` stayed 0 and the test warned and passed.  A test that
    // passes by not running is the shape this tree keeps designing out, and the
    // substitution exists to end this one.
    const Ephemeris eph = open_or_fail(ODL_DE440T_BSP, "de440t-spk");
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
    // No escape hatch any more.  The kernel is required to carry the record and
    // the comparison is required to happen, because the whole reason de440t was
    // substituted is that tdb_minus_tt has no other independent check in this
    // tree.
    INFO(n << " epochs; worst difference " << worst_ns << " ns");
    REQUIRE(n > 50);
    WARN("EPH-A-007: " << n << " epochs over a year, worst |kernel - series| = " << worst_ns
         << " ns, against a 100 ns budget");
    REQUIRE(worst_ns < 100.0);
}

TEST_CASE("EPH-A-009/014: refusals", "[eph][refusal]") {
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    SECTION("EPH-F-002: outside the coverage of that body in that kernel") {
        const auto cov = eph.coverage(Body::Sun);
        REQUIRE(cov.has_value());
        const auto r = eph.relative_state(Body::Sun, Body::SolarSystemBarycentre,
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
    const Ephemeris b = open_or_fail(ODL_DE440T_BSP, "de440t-spk");
    for (int i = 0; i < 3; ++i) {
        const auto ca = a.coverage(Body::Sun);
        const auto cb = b.coverage(Body::Sun);
        REQUIRE(ca.has_value());
        REQUIRE(cb.has_value());
        REQUIRE(cb->first_jd_tdb < ca->first_jd_tdb);   // the full kernel starts earlier
    }
}

// EPH-A-017 / PERT-Q-010: the centre is not a runtime argument on a frame-tagged
// return.  Until 2026-09-18 `state()` handed back `State<Frame::BCRS>` whatever
// centre was asked for, so a geocentric vector was typed as barycentric — the
// frame in the type as FRAME-R-004 requires, and the ORIGIN in an argument the
// type did not carry.  Plan §5 constraint 10: what a value means belongs in its
// type, never in the argument that produced it.
TEST_CASE("EPH-A-017: the centre is in the type, not in an argument", "[eph][spec]") {
    const Ephemeris eph = open_or_fail(ODL_DE440S_BSP, "de440s-spk");
    const Epoch when = tdb_at(2458849.5);

    auto bary = eph.barycentric_state(Body::Moon, when, leaps());
    auto geo = eph.geocentric_state(Body::Moon, when, leaps());
    auto rel = eph.relative_state(Body::Moon, Body::JupiterBarycentre, when, leaps());
    REQUIRE(bary.has_value());
    REQUIRE(geo.has_value());
    REQUIRE(rel.has_value());

    // Three different types, and the compiler knows which is which.
    static_assert(std::is_same_v<decltype(bary)::value_type,
                                 odl::frames::State<odl::frames::Frame::BCRS>>);
    static_assert(std::is_same_v<decltype(geo)::value_type,
                                 odl::frames::State<odl::frames::Frame::GCRS>>);
    static_assert(!std::is_convertible_v<decltype(rel)::value_type,
                                         odl::frames::State<odl::frames::Frame::BCRS>>);
    static_assert(!std::is_convertible_v<decltype(rel)::value_type,
                                         odl::frames::State<odl::frames::Frame::GCRS>>);
    static_assert(!std::is_convertible_v<odl::frames::State<odl::frames::Frame::BCRS>,
                                         odl::frames::State<odl::frames::Frame::GCRS>>);

    // And they are different vectors: the Moon is ~3.8e5 km from the Earth and
    // ~1.5e8 km from the barycentre.
    INFO("barycentric " << bary->position().norm() << " km, geocentric "
         << geo->position().norm() << " km");
    REQUIRE(geo->position().norm() > 3.5e5);
    REQUIRE(geo->position().norm() < 4.1e5);
    REQUIRE(bary->position().norm() > 1.3e8);

    // The untagged one carries which bodies it is between, because nothing else
    // can say so once the frame tag is gone.
    CHECK(rel->target == Body::Moon);
    CHECK(rel->centre == Body::JupiterBarycentre);

    // The Earth about the Earth is identically zero and asking for it is a
    // mistake about which body was wanted, not a legitimate zero.
    auto self = eph.geocentric_state(Body::Earth, when, leaps());
    REQUIRE_FALSE(self.has_value());
    CHECK(self.error().id == "EPH-F-008");
}
