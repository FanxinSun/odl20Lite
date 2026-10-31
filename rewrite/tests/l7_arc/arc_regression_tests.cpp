// arc_regression_tests.cpp — L7 step 3, part 2: ARC-R-001 is a REGRESSION GUARD for CI.  IT IS NOT THE STEP'S EVIDENCE: the evidence is ARC-L-001 ([.long], the full three-day experiment,
// whose log is kept and hashed).  This case runs a ONE-DAY sub-arc of the gate arc (same truth, model, stations and ladder construction) at rungs s = 1 and 30 and asserts what was registered
// before its first run (amendment 3): LM at tau = 1e-3 ends converged_* (not stalled, max_iterations or refused) and within 1 m and 1 mm/s of the truth; its cost is bounded by the ctest
// TIMEOUT of 300 s (tests/CMakeLists.txt).
#include "arc_runs.hpp"

using namespace l7arc;

TEST_CASE("ARC-R-001  REGRESSION GUARD, not the step's evidence: LM on a one-day sub-arc, rungs 1 and 30", "[arc][regression]") {
    ArcForces forces;
    const ArcDesign d = gate_arc_design(1.0);
    const auto truth = cartesian_of(d);
    const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
    ArcModel arc(forces, d, grid, kArcGridStepS);
    arc.generate_data(truth, 0, 0.0);
    REQUIRE(arc.count() > 6);
    std::size_t rungs = 0;
    for (double s : {1.0, 30.0}) {
        const auto l = run_lm(arc, truth, rung_start(truth, s));
        print_lm(d.name, s, l, arc.count());
        const auto st = l.result.status;
        CHECK((st == odl::estimation::LmStatus::converged_gradient || st == odl::estimation::LmStatus::converged_residual || st == odl::estimation::LmStatus::converged_step));
        CHECK(l.error.within_gate());
        ++rungs;
    }
    CHECK(rungs == 2);    // rule 3: the loop ran
}
