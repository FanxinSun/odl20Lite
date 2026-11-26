// prior_frames_tests.cpp — SPEC-estimation.md §8.4, EST-A-403 (d): the integration comparison.  This target links `frames` AND `estimation`, so that `modules/estimation` itself stays `core`-only
// (EST-Q-008, ruled 2026-11-27): the nine doubles of frames::rtn_basis(r, v) are compared ELEMENT FOR ELEMENT with the formula of FRAME-R-040 written here (e_R = r/|r|, e_N = (r x v)/|r x v|,
// e_T = e_N x e_R; plain double, division, no reciprocal), to 4u, for the fixture state and for ten seeded circular and eccentric states; a rectilinear state is refused (FRAME-F-004), so no block
// can be built from it; and every basis frames returns is accepted by PriorBlock::rtn (orthonormal to the registered 1e-12).  REGISTERED before step 4's code existed (v0.5, d2df6aa).

#include <odl/core/vec3.hpp>
#include <odl/estimation/prior.hpp>
#include <odl/frames/local.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

constexpr double kU = 0x1p-53;                                           // the unit roundoff of binary64

struct Rng {                                                              // splitmix64, seed 20261127 (the fixture of §8.4)
    std::uint64_t s = 20261127ULL;
    std::uint64_t next() {
        s += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uni() { return static_cast<double>(next() >> 11) * 0x1p-52 - 1.0; }      // [-1, 1)
    double unit01() { return 0.5 * (uni() + 1.0); }
};

std::array<double, 9> formula(const odl::Vec3& r, const odl::Vec3& v) {
    const double rn = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    const double hx = r.y * v.z - r.z * v.y, hy = r.z * v.x - r.x * v.z, hz = r.x * v.y - r.y * v.x;
    const double hn = std::sqrt(hx * hx + hy * hy + hz * hz);
    const double er[3] = {r.x / rn, r.y / rn, r.z / rn}, en[3] = {hx / hn, hy / hn, hz / hn};
    const double et[3] = {en[1] * er[2] - en[2] * er[1], en[2] * er[0] - en[0] * er[2], en[0] * er[1] - en[1] * er[0]};
    return {er[0], er[1], er[2], et[0], et[1], et[2], en[0], en[1], en[2]};
}

std::array<double, 9> nine(const odl::Mat3& m) {
    return {m.r[0][0], m.r[0][1], m.r[0][2], m.r[1][0], m.r[1][1], m.r[1][2], m.r[2][0], m.r[2][1], m.r[2][2]};
}

}  // namespace

TEST_CASE("EST-A-403d: frames::rtn_basis equals the FRAME-R-040 formula element for element to 4u; a rectilinear state is refused; the basis is accepted by PriorBlock::rtn",
          "[estimation][prior][a403d]") {
    // the states: the fixture, then ten seeded ones (circular, and eccentric through the flight-path angle: e_T is NOT the velocity direction, FRAME-R-042)
    std::vector<std::pair<odl::Vec3, odl::Vec3>> states;
    states.push_back({odl::Vec3{6878.137, 0.0, 0.0}, odl::Vec3{0.0, 5.381, 5.381}});
    Rng g;
    for (int i = 0; i < 10; ++i) {
        const double radius = 6600.0 + 35000.0 * g.unit01();
        const double speed = 3.0 + 5.0 * g.unit01();
        odl::Vec3 u{g.uni(), g.uni(), g.uni()};
        u = (1.0 / u.norm()) * u;
        odl::Vec3 w{g.uni(), g.uni(), g.uni()};
        odl::Vec3 n = u.cross(w);
        n = (1.0 / n.norm()) * n;
        const odl::Vec3 t = n.cross(u);
        const double e = (i < 3) ? 0.0 : 0.7 * g.unit01();                    // three circular, seven eccentric
        const double nu = 2.0 * 3.14159265358979323846 * g.unit01();
        const double gamma = std::atan(e * std::sin(nu) / (1.0 + e * std::cos(nu)));
        const odl::Vec3 r = radius * u;
        const odl::Vec3 v = speed * (std::sin(gamma) * u + std::cos(gamma) * t);
        states.push_back({r, v});
    }
    REQUIRE(states.size() == 11);

    double worst = 0.0;
    for (std::size_t s = 0; s < states.size(); ++s) {
        const auto& [r, v] = states[s];
        const auto got = odl::frames::rtn_basis(r, v);
        REQUIRE(got.has_value());
        const std::array<double, 9> a = nine(*got), b = formula(r, v);
        double dev = 0.0;
        for (std::size_t i = 0; i < 9; ++i) dev = std::max(dev, std::fabs(a[i] - b[i]));
        worst = std::max(worst, dev);
        CHECK(dev <= 4.0 * kU);
        // the module accepts it: orthonormal to the registered 1e-12, three names
        const auto blk = odl::estimation::PriorBlock::rtn({"x", "y", "z"}, {0.0, 0.0, 0.0}, a, {0.2, 200.0, 0.2});
        CHECK(blk.has_value());
    }
    std::printf("PRIOR 403d %zu states (the fixture and ten seeded): worst element difference between frames::rtn_basis and the FRAME-R-040 formula %.3e (bound 4u = %.3e)\n", states.size(), worst, 4.0 * kU);

    // a rectilinear state (r parallel to v) and a zero velocity are refused with FRAME-F-004: no basis, so no block can be built
    const auto rect = odl::frames::rtn_basis(odl::Vec3{7000.0, 0.0, 0.0}, odl::Vec3{1.0, 0.0, 0.0});
    REQUIRE(!rect.has_value());
    CHECK(std::string(rect.error().id) == "FRAME-F-004");
    const auto zero = odl::frames::rtn_basis(odl::Vec3{7000.0, 0.0, 0.0}, odl::Vec3{0.0, 0.0, 0.0});
    REQUIRE(!zero.has_value());
    CHECK(std::string(zero.error().id) == "FRAME-F-004");
}
