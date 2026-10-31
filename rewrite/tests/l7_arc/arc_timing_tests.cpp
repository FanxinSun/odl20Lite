// arc_timing_tests.cpp — L7 step 3, part 2: wall-time measurements of the arc's propagation (hidden, [.timing]); they size the schedule and the span, nothing else.
#include "arc_fixture.hpp"

#include <chrono>
#include <cstdio>

using namespace l7arc;

namespace {
double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
}  // namespace

TEST_CASE("ARC-X-001  propagation wall times", "[.timing]") {
    const double Re = 6378137.0;
    ArcDesign gate{"gate", 2025, 12, 20, 0, Re + 720e3, 0.001, 98.27, 0.0, 90.0, 0.0, 3.0, 30.0};
    ArcForces forces;
    const odl::time::Epoch t0 = epoch_of(gate);
    const auto x0 = state_from(t0, cartesian_of(gate));
    for (double span : {3600.0, 21600.0}) {
        const double w0 = now_s();
        auto p = dyn::propagate(forces.forces(), forces.params(), x0, span, kPropagationTolerance);
        const double w1 = now_s();
        REQUIRE(p.has_value());
        auto q = dyn::propagate_with_stm(forces.forces(), forces.params(), x0, span, kPropagationTolerance);
        const double w2 = now_s();
        REQUIRE(q.has_value());
        std::printf("ARCTIME span_s=%.0f propagate=%.3f s stm=%.3f s\n", span, w1 - w0, w2 - w1);
    }
    // one force evaluation, repeated
    {
        const int n = 200;
        const double w0 = now_s();
        for (int k = 0; k < n; ++k) {
            auto c = forces.forces().contributions_at(t0, x0, forces.params());
            REQUIRE(c.has_value());
        }
        const double w1 = now_s();
        std::printf("ARCTIME contributions_at = %.3f ms\n", (w1 - w0) / n * 1e3);
    }
    // the 30-second chain
    {
        const double w0 = now_s();
        auto g = state_grid(forces, x0, 3600.0, 30.0);
        const double w1 = now_s();
        std::printf("ARCTIME 120 steps of 30 s: %.3f s (%zu states)\n", w1 - w0, g.size());
    }
}

TEST_CASE("ARC-X-002  tolerance against cost and error (one day, against a tight reference)", "[.timing]") {
    const double Re = 6378137.0;
    ArcDesign gate{"gate", 2025, 12, 20, 0, Re + 720e3, 0.001, 98.27, 0.0, 90.0, 0.0, 3.0, 30.0};
    ArcForces forces;
    const odl::time::Epoch t0 = epoch_of(gate);
    const auto x0 = state_from(t0, cartesian_of(gate));
    const double span = 86400.0;
    auto ref = dyn::propagate(forces.forces(), forces.params(), x0, span, 1e-13);
    REQUIRE(ref.has_value());
    const auto rr = metres_of(*ref);
    for (double tol : {1e-12, 1e-11, 1e-10, 1e-9, 1e-8}) {
        const double w0 = now_s();
        auto p = dyn::propagate_with_stm(forces.forces(), forces.params(), x0, span, tol);
        const double w1 = now_s();
        REQUIRE(p.has_value());
        const auto pp = metres_of(p->state);
        const double dr = std::sqrt((pp[0] - rr[0]) * (pp[0] - rr[0]) + (pp[1] - rr[1]) * (pp[1] - rr[1]) + (pp[2] - rr[2]) * (pp[2] - rr[2]));
        std::printf("ARCTIME tol=%.0e stm_1day=%.3f s position_error_vs_1e-13=%.3e m\n", tol, w1 - w0, dr);
    }
}
