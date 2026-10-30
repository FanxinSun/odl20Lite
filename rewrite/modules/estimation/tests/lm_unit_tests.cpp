// lm_unit_tests.cpp — SPEC-estimation.md §8.3: the behaviour of the Levenberg–Marquardt driver on problems small enough to be understood: the statuses, the rejected trial, the refusal, the
// objective override, the final system, and the INVARIANCE the manager asked for on 2026-10-31 (residuals and parameters rescaled by powers of two give the same iterates and the same status).
#include <odl/estimation/levenberg_marquardt.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>

using namespace odl::estimation;

namespace {

// y = a exp(-b t) at t = 0..7, data generated from (2, 0.5) plus a fixed small deviation so that the problem is not consistent
const double kT[8] = {0, 1, 2, 3, 4, 5, 6, 7};
const double kY[8] = {2.05, 1.18, 0.69, 0.42, 0.25, 0.16, 0.08, 0.06};

LmModel decay() {
    return [](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(8);
        if (jac) e.jacobian.assign(16, 0.0);
        for (std::size_t k = 0; k < 8; ++k) {
            const double ex = std::exp(-p[1] * kT[k]);
            e.residual[k] = kY[k] - p[0] * ex;
            if (jac) {
                e.jacobian[2 * k] = ex;
                e.jacobian[2 * k + 1] = -p[0] * kT[k] * ex;
            }
        }
        return e;
    };
}

const std::vector<std::string> kNames2 = {"a", "b"};

}  // namespace

TEST_CASE("EST-A-310: a linear problem is solved in one accepted step and says why it stopped", "[estimation][lm][unit]") {
    // r = y - (c0 + c1 t): the Jacobian is constant
    const LmModel line = [](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(8);
        if (jac) e.jacobian.assign(16, 0.0);
        for (std::size_t k = 0; k < 8; ++k) {
            e.residual[k] = kY[k] - (p[0] + p[1] * kT[k]);
            if (jac) {
                e.jacobian[2 * k] = 1.0;
                e.jacobian[2 * k + 1] = kT[k];
            }
        }
        return e;
    };
    const double x0[2] = {0.0, 0.0};
    LmOptions o;
    o.tau = 1.0e-12;
    const LmResult r = levenberg_marquardt(kNames2, x0, line, o);
    CHECK((r.status == LmStatus::converged_gradient || r.status == LmStatus::converged_step));
    CHECK(r.accepted_steps >= 1);
    // the least-squares line, from the normal equations written out
    double st = 0, stt = 0, sy = 0, sty = 0;
    for (std::size_t k = 0; k < 8; ++k) { st += kT[k]; stt += kT[k] * kT[k]; sy += kY[k]; sty += kT[k] * kY[k]; }
    const double det = 8.0 * stt - st * st;
    const double c1 = (8.0 * sty - st * sy) / det;
    const double c0 = (sy - c1 * st) / 8.0;
    CHECK(std::fabs(r.x[0] - c0) <= 1.0e-10 * std::fabs(c0));
    CHECK(std::fabs(r.x[1] - c1) <= 1.0e-10 * std::fabs(c1));
    REQUIRE(r.final_system.has_value());
    CHECK(r.final_system->degrees_of_freedom == 6);
    CHECK(std::string(to_string(r.status)).rfind("converged", 0) == 0);
}

TEST_CASE("EST-A-311: a nonlinear fit converges, from a start where the plain iteration would not, with every trial accounted for", "[estimation][lm][unit]") {
    const double x0[2] = {10.0, 3.0};
    const LmResult r = levenberg_marquardt(kNames2, x0, decay());
    CHECK((r.status == LmStatus::converged_gradient || r.status == LmStatus::converged_step));
    CHECK(r.accepted_steps + r.rejected_steps == r.iterations - (r.status == LmStatus::converged_step ? 1u : 0u));
    CHECK(r.history.size() == r.iterations);
    CHECK(r.cosine <= 1.0e-4);
    CHECK(std::fabs(r.x[0] - 2.0) < 0.3);
    CHECK(std::fabs(r.x[1] - 0.5) < 0.1);
    // every accepted pass had a positive gain ratio, every rejected one did not
    for (const LmIteration& it : r.history) {
        if (it.accepted) CHECK(it.rho > 0.0);
        else CHECK(!(it.rho > 0.0));
    }
}

TEST_CASE("EST-A-312: a step stop while the gradient is large is `stalled`, never converged", "[estimation][lm][unit]") {
    const double x0[2] = {10.0, 3.0};
    LmOptions o;
    o.step_tolerance = 0.5;                          // a step test that fires at the first step: the gradient there is far from zero
    const LmResult r = levenberg_marquardt(kNames2, x0, decay(), o);
    CHECK(r.status == LmStatus::stalled);
    CHECK(r.cosine > o.stalled_cosine);
    CHECK(r.message.find("gradient cosine") != std::string::npos);
}

TEST_CASE("EST-A-313: the model refusing the start is `refused`; a trial the model refuses or answers with a non-finite number is a rejected step, not an abort", "[estimation][lm][unit]") {
    const double x0[2] = {2.0, 0.5};
    {
        const LmModel never = [](std::span<const double>, bool) -> odl::Result<LmEvaluation, EstimationRefusal> { return odl::err(EstimationRefusal{"TST-F-001", "outside the domain"}); };
        const LmResult r = levenberg_marquardt(kNames2, x0, never);
        CHECK(r.status == LmStatus::refused);
        CHECK(r.message.find("starting point") != std::string::npos);
        CHECK(r.iterations == 0);
    }
    {   // the second and third evaluations answer NaN (a trial that leaves the model's domain): those trials are rejected, the iteration goes on and converges
        LmModel base = decay();
        int calls = 0, nan_trials = 0;
        const LmModel bounded = [&](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            auto e = base(p, jac);
            if (++calls == 2 || calls == 3 || calls == 4) {
                ++nan_trials;
                e->residual[0] = std::numeric_limits<double>::quiet_NaN();
            }
            return e;
        };
        const double far[2] = {10.0, 3.0};
        const LmResult r = levenberg_marquardt(kNames2, far, bounded);
        CHECK((r.status == LmStatus::converged_gradient || r.status == LmStatus::converged_step));
        CHECK(nan_trials >= 1);
        CHECK(r.rejected_steps >= 1);
    }
    {   // every trial refused after the start: mu climbs, the step shrinks, and with the DEFAULT step test the run stops `stalled` (the gradient is large); with the step test disabled mu reaches mu_max and it is `refused`
        LmModel base = decay();
        bool first = true;
        const LmModel only_start = [&](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            if (first) { first = false; return base(p, jac); }
            return odl::err(EstimationRefusal{"TST-F-002", "outside the domain"});
        };
        const double far[2] = {10.0, 3.0};
        const LmResult stalled = levenberg_marquardt(kNames2, far, only_start);
        CHECK(stalled.status == LmStatus::stalled);
        CHECK(stalled.rejected_steps >= 8);
        first = true;
        LmOptions o;
        o.step_tolerance = 0.0;
        const LmResult r = levenberg_marquardt(kNames2, far, only_start, o);
        CHECK(r.status == LmStatus::refused);
        CHECK(r.message.find("mu_max") != std::string::npos);
        CHECK(r.rejected_steps >= 8);                 // mu is multiplied by 2, 4, 8, ...: the product is 2^(k(k+1)/2), so 1e16 is passed after about ten rejections
    }
}

TEST_CASE("EST-A-314: the iteration limit, the zero-residual test, and an objective carried by the model", "[estimation][lm][unit]") {
    const double x0[2] = {10.0, 3.0};
    {
        LmOptions o;
        o.max_iterations = 2;
        const LmResult r = levenberg_marquardt(kNames2, x0, decay(), o);
        CHECK(r.status == LmStatus::max_iterations);
        CHECK(r.iterations == 2);
    }
    {   // a consistent model: the data are exactly a exp(-b t) at (2, 0.5)
        const LmModel exact = [](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            LmEvaluation e;
            e.residual.resize(8);
            if (jac) e.jacobian.assign(16, 0.0);
            for (std::size_t k = 0; k < 8; ++k) {
                const double ex = std::exp(-p[1] * kT[k]);
                e.residual[k] = 2.0 * std::exp(-0.5 * kT[k]) - p[0] * ex;
                if (jac) {
                    e.jacobian[2 * k] = ex;
                    e.jacobian[2 * k + 1] = -p[0] * kT[k] * ex;
                }
            }
            return e;
        };
        const double s[2] = {1.5, 0.3};
        const LmResult r = levenberg_marquardt(kNames2, s, exact);
        CHECK((r.status == LmStatus::converged_residual || r.status == LmStatus::converged_gradient));
        CHECK(std::fabs(r.x[0] - 2.0) < 1.0e-8);
        CHECK(std::fabs(r.x[1] - 0.5) < 1.0e-8);
    }
    {   // an additive constant in the carried objective changes no difference of it: the same iterates
        LmModel base = decay();
        const LmModel offset = [&](std::span<const double> p, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            auto e = base(p, jac);
            double s = 0.0;
            for (double r : e->residual) s += r * r;
            e->objective = 0.5 * s + 1000.0;
            return e;
        };
        const LmResult a = levenberg_marquardt(kNames2, x0, base);
        const LmResult b = levenberg_marquardt(kNames2, x0, offset);
        // F + 1000 rounds at 1e-13 relative, so the gain ratios agree where the decreases are large and drift where they are not: the first five passes are compared
        REQUIRE(a.history.size() >= 5);
        REQUIRE(b.history.size() >= 5);
        for (std::size_t i = 0; i < 5; ++i) {
            CHECK(a.history[i].accepted == b.history[i].accepted);
            CHECK(std::fabs(a.history[i].mu - b.history[i].mu) <= 1.0e-6 * a.history[i].mu);
        }
        CHECK(std::fabs(b.objective - a.objective - 1000.0) <= 1.0e-9);
    }
}

TEST_CASE("EST-A-315: INVARIANCE -- residuals and parameters rescaled by powers of two give the same iterates, the same status and the same damping", "[estimation][lm][unit][invariance]") {
    const double x0[2] = {10.0, 3.0};
    const LmResult ref = levenberg_marquardt(kNames2, x0, decay());
    // new parameters w with p = diag(2^k) w, residuals multiplied by 2^q
    for (const auto& kq : std::vector<std::array<int, 3>>{{{3, -2, 5}}, {{-7, 11, -4}}, {{0, 20, 0}}, {{-15, 0, 9}}}) {
        const double s0 = std::ldexp(1.0, kq[0]), s1 = std::ldexp(1.0, kq[1]), qf = std::ldexp(1.0, kq[2]);
        LmModel base = decay();
        const LmModel scaled = [&](std::span<const double> w, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
            const double p[2] = {s0 * w[0], s1 * w[1]};
            auto e = base(p, jac);
            for (double& r : e->residual) r *= qf;
            if (jac)
                for (std::size_t k = 0; k + 1 < e->jacobian.size(); k += 2) {
                    e->jacobian[k] *= qf * s0;
                    e->jacobian[k + 1] *= qf * s1;
                }
            return e;
        };
        const double w0[2] = {x0[0] / s0, x0[1] / s1};
        const LmResult r = levenberg_marquardt(kNames2, w0, scaled);
        INFO("k = " << kq[0] << ", " << kq[1] << ", q = " << kq[2]);
        CHECK(r.status == ref.status);
        REQUIRE(r.history.size() == ref.history.size());
        for (std::size_t i = 0; i < ref.history.size(); ++i) {
            CHECK(r.history[i].accepted == ref.history[i].accepted);
            CHECK(r.history[i].mu == ref.history[i].mu);          // bit for bit
            CHECK(r.history[i].rho == ref.history[i].rho);
            CHECK(r.history[i].cosine == ref.history[i].cosine);
            CHECK(r.history[i].objective == ref.history[i].objective * qf * qf);
        }
        CHECK(s0 * r.x[0] == ref.x[0]);
        CHECK(s1 * r.x[1] == ref.x[1]);
    }
}
