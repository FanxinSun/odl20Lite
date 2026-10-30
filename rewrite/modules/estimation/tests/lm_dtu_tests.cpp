// lm_dtu_tests.cpp — SPEC-estimation.md §8.3, EST-A-320: MEYER'S PROBLEM as printed in DTU's note (Madsen, Nielsen & Tingleff, IMM-3215, 2004, Example 3.19), in both formulations.
//   x-form  f_i(x) = y_i - x1 exp(x2 / (t_i + x3)), t_i = 45 + 5 i, i = 1..16, y_i as printed;                          start x0 = [2e-2, 4e3, 2.5e2]
//   z-form  phi_i(z) = 1e-3 y_i - z1 exp(10 z2 / (u_i + z3) - 13), u_i = 0.45 + 0.05 i,  z = [1e-3 e^13 x1, 1e-3 x2, 1e-2 x3]; start z0 = [8.85, 4, 2.5]
// DTU's values for this example, as printed: Algorithm 3.16 with tau = 1, eps1 = 1e-6, eps2 = 1e-10; "the iteration is stopped by (3.15b) after 175 iteration steps with the first
// formulation, and by (3.15a) after 88 steps with the well-scaled reformulation"; minimiser x* ~ [5.61e-3, 6.18e3, 3.45e2] with F(x*) ~ 43.97, z* ~ [2.48, 6.18, 3.45] with Phi(z*) ~ 4.397e-5.
// What is gated (registered before the first run, L7_step3_registration.txt): (1) the identity F(x) = 1e6 Phi(z(x)) at points (a transcription check of the reformulation); (2) DTU's UNSCALED
// algorithm, written in lm_controls.hpp, stops by (3.15a) after exactly 88 passes on the z-form; (3) both formulations end converged_* under the library's scaled algorithm at the same minimum.
// REPORTED, not gated: DTU's 175 on the x-form; the library's pass counts; and how far the scaled algorithm's iterates on the two formulations are from each other after the change of variables.
#include "lm_controls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>

using namespace odl::estimation;
using namespace odl::estimation::fx;

namespace {

const double kY[16] = {34780, 28610, 23650, 19630, 16370, 13720, 11540, 9744, 8261, 7030, 6005, 5147, 4427, 3820, 3307, 2872};

LmModel meyer_x() {
    return [](std::span<const double> x, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(16);
        if (jac) e.jacobian.assign(48, 0.0);
        for (std::size_t i = 0; i < 16; ++i) {
            const double t = 45.0 + 5.0 * static_cast<double>(i + 1);
            const double d = t + x[2];
            const double ex = std::exp(x[1] / d);
            e.residual[i] = kY[i] - x[0] * ex;
            if (jac) {
                e.jacobian[3 * i + 0] = ex;
                e.jacobian[3 * i + 1] = x[0] * ex / d;
                e.jacobian[3 * i + 2] = -x[0] * ex * x[1] / (d * d);
            }
        }
        return e;
    };
}

LmModel meyer_z() {
    return [](std::span<const double> z, bool jac) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(16);
        if (jac) e.jacobian.assign(48, 0.0);
        for (std::size_t i = 0; i < 16; ++i) {
            const double u = 0.45 + 0.05 * static_cast<double>(i + 1);
            const double d = u + z[2];
            const double ex = std::exp(10.0 * z[1] / d - 13.0);
            e.residual[i] = 1.0e-3 * kY[i] - z[0] * ex;
            if (jac) {
                e.jacobian[3 * i + 0] = ex;
                e.jacobian[3 * i + 1] = z[0] * ex * 10.0 / d;
                e.jacobian[3 * i + 2] = -z[0] * ex * 10.0 * z[1] / (d * d);
            }
        }
        return e;
    };
}

std::vector<double> z_of_x(const std::vector<double>& x) { return {1.0e-3 * std::exp(13.0) * x[0], 1.0e-3 * x[1], 1.0e-2 * x[2]}; }

double objective_of(const LmModel& m, const std::vector<double>& p) {
    auto e = m(p, false);
    double s = 0.0;
    for (double r : e->residual) s += r * r;
    return 0.5 * s;
}

}  // namespace

TEST_CASE("EST-A-320: Meyer's problem -- the reformulation is the same problem; DTU's unscaled algorithm gives its printed 88; the scaled algorithm reaches the same minimum on both", "[estimation][lm][dtu]") {
    const std::vector<std::string> names = {"p1", "p2", "p3"};
    const LmModel fx_ = meyer_x(), fz = meyer_z();
    // (1) the identity, at several points
    for (const std::vector<double>& x : std::vector<std::vector<double>>{{2.0e-2, 4.0e3, 2.5e2}, {5.61e-3, 6.18e3, 3.45e2}, {1.0e-2, 5.0e3, 3.0e2}, {3.0e-3, 7.0e3, 4.0e2}}) {
        const double a = objective_of(fx_, x), b = 1.0e6 * objective_of(fz, z_of_x(x));
        INFO("x = " << x[0] << ", " << x[1] << ", " << x[2]);
        CHECK(std::fabs(a - b) <= 1.0e-12 * a);
    }
    // the printed starting vectors correspond: z0 = z(x0) to the printed digits
    const std::vector<double> x0 = {2.0e-2, 4.0e3, 2.5e2}, z0 = {8.85, 4.0, 2.5};
    const std::vector<double> zz = z_of_x(x0);
    CHECK(std::fabs(zz[0] - 8.85) < 0.005);
    CHECK(zz[1] == 4.0);
    CHECK(zz[2] == 2.5);

    // (2) DTU's Algorithm 3.16, unscaled, with DTU's printed parameters
    const ControlRun uz = dtu_unscaled(names, z0, fz, 1.0, 1.0e-6, 1.0e-10, 1000);
    const ControlRun ux = dtu_unscaled(names, x0, fx_, 1.0, 1.0e-6, 1.0e-10, 1000);
    std::printf("DTU z-form: ended by %s after %zu passes (%zu accepted), Phi=%.6e, z=[%.5g %.5g %.5g]   [DTU prints: (3.15a) after 88]\n", uz.ended.c_str(), uz.passes, uz.iterations, uz.objective, uz.x[0],
                uz.x[1], uz.x[2]);
    std::printf("DTU x-form: ended by %s after %zu passes (%zu accepted), F=%.6e, x=[%.5g %.5g %.5g]   [DTU prints: (3.15b) after 175; reported, not gated]\n", ux.ended.c_str(), ux.passes, ux.iterations,
                ux.objective, ux.x[0], ux.x[1], ux.x[2]);
    CHECK(uz.ended == "gradient");
    CHECK(uz.passes == 88);

    // (3) the scaled algorithm on both formulations
    LmOptions o;
    o.tau = 1.0;
    const LmResult lz = levenberg_marquardt(names, z0, fz, o);
    const LmResult lx = levenberg_marquardt(names, x0, fx_, o);
    std::printf("scaled LM z-form: %s, %zu passes (%zu accepted, %zu rejected), Phi=%.6e, z=[%.5g %.5g %.5g]\n", to_string(lz.status), lz.iterations, lz.accepted_steps, lz.rejected_steps, lz.objective,
                lz.x[0], lz.x[1], lz.x[2]);
    std::printf("scaled LM x-form: %s, %zu passes (%zu accepted, %zu rejected), F=%.6e, x=[%.5g %.5g %.5g]\n", to_string(lx.status), lx.iterations, lx.accepted_steps, lx.rejected_steps, lx.objective, lx.x[0],
                lx.x[1], lx.x[2]);
    CHECK((lz.status == LmStatus::converged_gradient || lz.status == LmStatus::converged_step));
    CHECK((lx.status == LmStatus::converged_gradient || lx.status == LmStatus::converged_step));
    const std::vector<double> zx = z_of_x(lx.x);
    for (std::size_t i = 0; i < 3; ++i) CHECK(std::fabs(zx[i] - lz.x[i]) <= 1.0e-6 * std::fabs(lz.x[i]));
    CHECK(std::fabs(lx.objective - 1.0e6 * lz.objective) <= 1.0e-6 * lx.objective);
    // the published minimum: F(x*) ~ 43.97 and Phi(z*) ~ 4.397e-5, to the printed four digits
    CHECK(std::fabs(lx.objective - 43.97) <= 0.005);
    CHECK(std::fabs(lz.objective - 4.397e-5) <= 0.0005e-5);
    // reported: the scaled iterates of the two formulations, mapped to one another
    const std::size_t common = std::min(lx.accepted_iterates.size(), lz.accepted_iterates.size());
    double worst = 0.0;
    for (std::size_t k = 0; k < common; ++k) {
        const std::vector<double> mapped = z_of_x(lx.accepted_iterates[k]);
        for (std::size_t i = 0; i < 3; ++i) worst = std::max(worst, std::fabs(mapped[i] - lz.accepted_iterates[k][i]) / std::fabs(lz.accepted_iterates[k][i]));
    }
    std::printf("scaled iterates, x-form mapped to z against the z-form: %zu common accepted steps, largest relative difference %.3e\n", common, worst);
}
