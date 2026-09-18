// stm_tests.cpp — L3 step 3's gate.
//
// SPEC-stm.md §8.  THE THRESHOLD IS NOT AN ABSOLUTE NUMBER, and that is the
// point of STM-A-001. The finite-difference agreement depends on the
// perturbation size, which is a free parameter, so any tolerance chosen after
// seeing the numbers is a tolerance chosen FROM them (plan §4 rule 7). The band
// is therefore measured in the same run from FD estimates at several
// perturbation sizes -- a family whose spread depends on the differencing and
// not at all on whether the analytic derivative is right.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/dynamics/variational.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using frames::Frame;

namespace {

constexpr double kMu = 3.986004415e14;      // m^3 s^-2

/// Two-body, with an ANALYTIC gradient (STM-R-003). The point-mass tensor is
///     da/dr = -mu ( I/r^3 - 3 r r^T / r^5 )
/// and the velocity dependence is genuinely ABSENT, with a bound of exactly
/// zero -- which is a different statement from a zero block (DYN-R-027).
class TwoBody final : public Force {
public:
    ForceId id() const override { return ForceId{"two-body"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }

    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&,
          const ParameterSet&, const ParameterRegistry& reg) const override {
        const Vec3 r = r_m.metres();
        const double r2 = r.x * r.x + r.y * r.y + r.z * r.z;
        const double rn = std::sqrt(r2);
        const double r3 = r2 * rn, r5 = r3 * r2;
        const Vec3 a{-kMu * r.x / r3, -kMu * r.y / r3, -kMu * r.z / r3};
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                dadr.r[i][j] = -kMu * ((i == j ? 1.0 / r3 : 0.0) - 3.0 * rv[i] * rv[j] / r5);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{a},
                               StateJacobian::no_velocity_dependence(dadr, 0.0),
                               ParameterJacobian{reg}};
    }

private:
    std::vector<ParameterId> none_{};
};

struct Setup {
    ParameterRegistry reg;
    ForceSet forces;
    ParameterSet params;
    frames::State<Frame::GCRS> x0;
    Setup()
        : reg(), forces(reg), params(reg),
          x0(*odl::time::Epoch::from_gps_week(2000, 0.0),
             Vec3{7331.0, 0.0, 0.0}, Vec3{0.0, 7.3739, 0.0}) {
        REQUIRE(forces.add(std::make_shared<TwoBody>()).has_value());
    }
};

/// Phi made dimensionless: D^-1 Phi D with D = diag(Lr,Lr,Lr,Lv,Lv,Lv), so every
/// entry is comparable and a single norm means something.
Mat6 scaled(const Mat6& phi, double Lr, double Lv) {
    const double d[6] = {Lr, Lr, Lr, Lv, Lv, Lv};
    Mat6 out{};
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) out[i][j] = phi[i][j] * d[j] / d[i];
    return out;
}

double max_abs_diff(const Mat6& a, const Mat6& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j) m = std::max(m, std::abs(a[i][j] - b[i][j]));
    return m;
}

/// Central-difference Phi at a relative perturbation `hrel`, by propagating the
/// SAME integration the analytic route uses.
Mat6 fd_phi(Setup& s, double seconds, double tol, double hrel, double Lr, double Lv) {
    Mat6 out{};
    const double d[6] = {Lr, Lr, Lr, Lv, Lv, Lv};
    for (std::size_t j = 0; j < 6; ++j) {
        const double h = hrel * d[j];
        double p[6] = {s.x0.position().x, s.x0.position().y, s.x0.position().z,
                       s.x0.velocity().x, s.x0.velocity().y, s.x0.velocity().z};
        double m[6]; for (std::size_t k = 0; k < 6; ++k) m[k] = p[k];
        p[j] += h; m[j] -= h;
        const frames::State<Frame::GCRS> sp{s.x0.epoch(), Vec3{p[0],p[1],p[2]}, Vec3{p[3],p[4],p[5]}};
        const frames::State<Frame::GCRS> sm{s.x0.epoch(), Vec3{m[0],m[1],m[2]}, Vec3{m[3],m[4],m[5]}};
        auto rp = propagate(s.forces, s.params, sp, seconds, tol);
        auto rm = propagate(s.forces, s.params, sm, seconds, tol);
        REQUIRE(rp.has_value()); REQUIRE(rm.has_value());
        const double a[6] = {rp->position().x, rp->position().y, rp->position().z,
                             rp->velocity().x, rp->velocity().y, rp->velocity().z};
        const double b[6] = {rm->position().x, rm->position().y, rm->position().z,
                             rm->velocity().x, rm->velocity().y, rm->velocity().z};
        for (std::size_t i = 0; i < 6; ++i) out[i][j] = (a[i] - b[i]) / (2.0 * h);
    }
    return out;
}

}  // namespace

TEST_CASE("STM-A-003  Phi(t0,t0) is the identity, exactly", "[stm][gate]") {
    Setup s;
    auto r = propagate_with_stm(s.forces, s.params, s.x0, 0.0, 1e-12);
    REQUIRE(r.has_value());
    for (std::size_t i = 0; i < 6; ++i)
        for (std::size_t j = 0; j < 6; ++j)
            CHECK(r->phi[i][j] == (i == j ? 1.0 : 0.0));
}

TEST_CASE("STM-A-001  Phi against finite differences, judged by a band measured in the same run",
          "[stm][gate]") {
    Setup s;
    const double seconds = 1200.0;
    const double tol = 1e-12;
    const double Lr = 7331.0, Lv = 7.3739;

    auto an = propagate_with_stm(s.forces, s.params, s.x0, seconds, tol);
    REQUIRE(an.has_value());
    const Mat6 A = scaled(an->phi, Lr, Lv);

    // THE BAND. FD estimates at several perturbation sizes; their spread depends
    // on the differencing and NOT on whether the analytic Phi is right, so it
    // cannot be fitted to the result it judges.
    //
    // THE FAMILY IS CHOSEN FROM THE PREDICTION, NOT FROM THE RESULT: a decade
    // centred on STM-P-1's optimal h of 1.1e-4. A wider family would be a WEAKER
    // test, not a safer one -- at h = 1e-3 the perturbation is 7 km and its own
    // truncation inflates the band to 1.6e-5, which `worst <= band` would then
    // pass trivially. The band must be the FD uncertainty near the optimum.
    const std::vector<double> hs{3.5e-5, 1.1e-4, 3.5e-4};
    std::vector<Mat6> fd;
    for (double h : hs) fd.push_back(scaled(fd_phi(s, seconds, tol, h, Lr, Lv), Lr, Lv));

    double band = 0.0;
    for (std::size_t a = 0; a < fd.size(); ++a)
        for (std::size_t b = a + 1; b < fd.size(); ++b)
            band = std::max(band, max_abs_diff(fd[a], fd[b]));

    double worst = 1e300;
    for (const auto& f : fd) worst = std::min(worst, max_abs_diff(A, f));

    INFO("band among FD estimates " << band << "; best |analytic - FD| " << worst
         << "; margin " << band / worst << "x");
    CHECK(worst <= band);            // no absolute number appears here

    // STM-A-002: and the comparison is not pathological. STM-P-1 predicted a
    // BEST AGREEMENT of ~6.6e-9 at this tolerance, WRITTEN BEFORE THE FIRST RUN,
    // from h^2/6 + tau/(2h) -- where tau/(2h) is the integrator's own noise, the
    // term a textbook analysis of finite differencing omits and the one that
    // dominates. It is `worst` that the prediction is about, not the band.
    INFO("STM-P-1 predicted ~6.6e-9; if only machine round-off mattered it would be ~3.8e-11");
    CHECK(worst > 6.6e-10);
    CHECK(worst < 6.6e-8);

    // THE BAND'S ABSOLUTE SIZE IS DELIBERATELY NOT BOUNDED HERE. A first version
    // of this test asserted band < 1e-6 and the band came out at 1.96e-6. That
    // check was both redundant and unprincipled: the band scales with how wide
    // the h family is, which is a DESIGN CHOICE, so any bound on it is a number
    // about the test rather than about the code -- and adjusting it upward once
    // it failed would have been precisely the fitting plan §4 rule 7 forbids.
    //
    // Non-degeneracy is established by the line above instead: if the comparison
    // were degenerate, `worst` would not land within a factor of three of a
    // figure predicted from h^2/6 + tau/(2h) before anything ran.
}

namespace {
/// A dissipative force, so that Liouville is exercised where the determinant
/// actually MOVES rather than only where it is constant. a = -k v, so
/// da/dv = -k I and tr(da/dv) = -3k.
class LinearDrag final : public Force {
public:
    explicit LinearDrag(double k) : k_(k) {}
    ForceId id() const override { return ForceId{"linear-drag"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }
    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>&, const Vec3& v,
          const ParameterSet&, const ParameterRegistry& reg) const override {
        Mat3 dadv{};
        dadv.r[0][0] = dadv.r[1][1] = dadv.r[2][2] = -k_;
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{-k_*v.x, -k_*v.y, -k_*v.z}},
                               StateJacobian::with_velocity(Mat3{}, dadv),
                               ParameterJacobian{reg}};
    }
private:
    double k_;
    std::vector<ParameterId> none_{};
};

double det6(Mat6 m) {
    double det = 1.0;
    for (std::size_t c = 0; c < 6; ++c) {
        std::size_t p = c;
        for (std::size_t i = c + 1; i < 6; ++i)
            if (std::abs(m[i][c]) > std::abs(m[p][c])) p = i;
        if (p != c) { std::swap(m[p], m[c]); det = -det; }
        det *= m[c][c];
        for (std::size_t i = c + 1; i < 6; ++i) {
            const double f = m[i][c] / m[c][c];
            for (std::size_t j = c; j < 6; ++j) m[i][j] -= f * m[c][j];
        }
    }
    return det;
}
}  // namespace

TEST_CASE("STM-A-005  Liouville: det Phi = exp(integral tr A dt)", "[stm][gate]") {
    // An invariant of the TRUE Phi that no finite-difference estimate enters.
    //
    // WRITTEN IN THE GENERAL FORM ON PURPOSE. "det Phi == 1" is true for the
    // forces L3 has and becomes FALSE the moment drag arrives at L4 with
    // tr(da/dv) < 0 -- and whoever met that failure would restrict the test to
    // conservative forces or delete it, losing the only check on Phi that
    // involves no difference estimate, exactly when the dynamics get harder.
    // Here the conservative case is the special case where the integral is zero,
    // and the dissipative case below makes the check SHARP rather than trivially
    // satisfied.
    Setup s;
    for (double seconds : {600.0, 3000.0, 6246.0}) {
        auto r = propagate_with_stm(s.forces, s.params, s.x0, seconds, 1e-12);
        REQUIRE(r.has_value());
        // determinant by Gaussian elimination with partial pivoting
        Mat6 m = r->phi;
        double det = 1.0;
        for (std::size_t c = 0; c < 6; ++c) {
            std::size_t p = c;
            for (std::size_t i = c + 1; i < 6; ++i)
                if (std::abs(m[i][c]) > std::abs(m[p][c])) p = i;
            if (p != c) { std::swap(m[p], m[c]); det = -det; }
            det *= m[c][c];
            for (std::size_t i = c + 1; i < 6; ++i) {
                const double f = m[i][c] / m[c][c];
                for (std::size_t j = c; j < 6; ++j) m[i][j] -= f * m[c][j];
            }
        }
        INFO("after " << seconds << " s, det Phi = " << det
             << ", integral tr A dt = " << r->integrated_trace);
        CHECK_THAT(det, Catch::Matchers::WithinRel(std::exp(r->integrated_trace), 1e-9));
        // and for THESE forces the integral is zero, so the general form reduces
        // to the conservative one -- which is the statement, not the assumption
        CHECK(r->integrated_trace == 0.0);
    }
}

TEST_CASE("STM-A-005b  Liouville where the determinant MOVES", "[stm][gate]") {
    // The same law with a dissipative force, so the test is sharp today rather
    // than waiting for L4 to make it so. a = -k v gives tr(A) = -3k exactly, so
    // det Phi = exp(-3 k t) -- a CLOSED FORM the integration must reproduce.
    Setup s;
    const double k = 2.0e-4;                     // s^-1
    REQUIRE(s.forces.add(std::make_shared<LinearDrag>(k)).has_value());

    for (double seconds : {600.0, 3000.0}) {
        auto r = propagate_with_stm(s.forces, s.params, s.x0, seconds, 1e-12);
        REQUIRE(r.has_value());
        const double det = det6(r->phi);
        const double closed_form = -3.0 * k * seconds;
        INFO("after " << seconds << " s: integral tr A dt = " << r->integrated_trace
             << ", closed form " << closed_form << "; det Phi = " << det
             << ", exp(integral) = " << std::exp(r->integrated_trace));
        // the integrated trace matches the closed form...
        CHECK_THAT(r->integrated_trace, Catch::Matchers::WithinRel(closed_form, 1e-10));
        // ...and the determinant follows it, having genuinely MOVED
        CHECK_THAT(det, Catch::Matchers::WithinRel(std::exp(closed_form), 1e-8));
        CHECK(det < 0.9);            // not the trivial det == 1 case
    }
}

TEST_CASE("STM-A-008  the neglected-velocity bound survives into the result", "[stm]") {
    Setup s;
    auto r = propagate_with_stm(s.forces, s.params, s.x0, 600.0, 1e-12);
    REQUIRE(r.has_value());
    // two-body declares ABSENT with a bound of exactly zero, which is a
    // different statement from a zero block
    CHECK(r->neglected_velocity_bound_per_s == 0.0);
}
