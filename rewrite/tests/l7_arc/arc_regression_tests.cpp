// arc_regression_tests.cpp — L7 step 3, part 2: ARC-R-001 is a REGRESSION GUARD for CI.  IT IS NOT THE STEP'S EVIDENCE: the evidence is ARC-L-001 ([.long], the full three-day experiment,
// whose log is kept and hashed).  This case runs a ONE-DAY sub-arc of the gate arc (same truth, model, stations) from rungs 1 and 3 of ladder P (s = 1 and 8.8; the old s = 1, 30 meant nothing
// once the ladder was found not evaluable) and asserts what was registered before its first run (amendments 3 and 4): each start EVALUATES; LM at tau = 1e-3 ends converged_*, or (amendment 5, after this case's first run failed on a wrong expectation) stalled WITH a final F below 1e-3 whitened (not
// max_iterations or refused) and within 1 m and 1 mm/s of the truth; its cost is bounded by the ctest TIMEOUT of 300 s (tests/CMakeLists.txt).
#include "arc_runs.hpp"

using namespace l7arc;

TEST_CASE("ARC-R-001  REGRESSION GUARD, not the step's evidence: LM on a one-day sub-arc, ladder P rungs 1 and 3", "[arc][regression]") {
    ArcForces forces;
    const ArcDesign d = gate_arc_design(1.0);
    const auto truth = cartesian_of(d);
    const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
    ArcModel arc(forces, d, grid, kArcGridStepS);
    arc.generate_data(truth, 0, 0.0);
    REQUIRE(arc.count() > 6);
    std::size_t rungs = 0;
    for (double s : {kRungsPGate[0], kRungsPGate[2]}) {
        const auto x0 = start_of(LadderKind::P, d, truth, s);
        REQUIRE(arc.evaluate(x0, false).has_value());            // evaluability first
        const auto l = run_lm(arc, truth, x0);
        print_lm(d.name, s, l, arc.count());
        const auto st = l.result.status;
        const bool converged = st == odl::estimation::LmStatus::converged_gradient || st == odl::estimation::LmStatus::converged_residual || st == odl::estimation::LmStatus::converged_step;
        // amendment 5: `stalled` passes ONLY with the final F below 1e-3 (whitened) -- on a noise-free arc the residual is the model's own noise and the cosine test measures nothing; refused and max_iterations fail
        const bool stalled_at_the_floor = st == odl::estimation::LmStatus::stalled && l.result.objective < 1.0e-3;
        CHECK((converged || stalled_at_the_floor));
        CHECK(l.error.within_gate());
        ++rungs;
    }
    CHECK(rungs == 2);    // rule 3: the loop ran
}

// ARC-R-002 — the crash's regression (registered before it ran, in L7_step3_registration.txt): ARC-X-009's state, forces (EGM2008 degree 2 + SRP) and tolerance; spans of 2 and 3 days must PROPAGATE in ONE call
// (they ended with SHDW-F-002 before the first-step cap of dyn::propagate).
TEST_CASE("ARC-R-002  one call over two and three days propagates (the first-step cap)", "[arc][regression]") {
    const ArcDesign d = gate_arc_design(3.0);
    const auto x0 = state_from(epoch_of(d), cartesian_of(d));
    ArcForces forces(2, false);
    std::size_t ran = 0;
    for (double days : {2.0, 3.0}) {
        auto r = dyn::propagate(forces.forces(), forces.params(), x0, days * 86400.0, kPropagationTolerance);
        INFO("span " << days << " d");
        REQUIRE(r.has_value());
        ++ran;
    }
    CHECK(ran == 2);
}
