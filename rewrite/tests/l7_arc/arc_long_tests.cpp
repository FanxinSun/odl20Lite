// arc_long_tests.cpp — L7 step 3, part 2: THE STEP'S EVIDENCE ON THE SAIL ARC, as registered (L7_step3_registration.txt, part 2 and amendment 3).
//
// HIDDEN ([.long]): it takes about two hours.  The ONE command that runs it, from the build tree (never a tag expression with "~", which is a union):
//     ./tests/odl_l7_arc_tests "ARC-L-001  the sail arc, as registered"
// and its log is the record (hash it: sha256sum).  Order: every Gauss-Newton rung of the gate arc; if none is classed DIVERGED the run STOPS here (a finding); then the eclipsing arc's
// Gauss-Newton rungs (recorded); then LM on both arcs, then the one seeded-noise LM run (gate arc, rung s = 10, seed 20251220).  The gate is the noise-free gate arc only.
#include "arc_runs.hpp"

using namespace l7arc;

TEST_CASE("ARC-L-001  the sail arc, as registered", "[.long]") {
    ArcForces forces;
    struct Prepared {
        ArcDesign design;
        std::array<double, 6> truth;
        std::vector<odl::frames::GcrsState> grid;
        std::unique_ptr<ArcModel> arc;
    };
    std::vector<Prepared> arcs;
    for (const ArcDesign& d : {gate_arc_design(3.0), eclipsing_arc_design(3.0)}) {
        Prepared p{d, cartesian_of(d), {}, nullptr};
        p.grid = state_grid(forces, state_from(epoch_of(d), p.truth), d.days * 86400.0, kArcGridStepS);
        p.arc = std::make_unique<ArcModel>(forces, d, p.grid, kArcGridStepS);
        p.arc->generate_data(p.truth, 0, 0.0);
        std::printf("ARCRUN data arc=%s observations=%zu schedule_hash=%s data_hash=%s\n", d.name, p.arc->count(), p.arc->schedule_hash().c_str(), p.arc->data_hash().c_str());
        std::fflush(stdout);
        arcs.push_back(std::move(p));
    }
    // phase 1: Gauss-Newton on the gate arc
    std::size_t diverged = 0, reached = 0;
    for (double s : kLadder) {
        const auto g = run_gauss_newton(*arcs[0].arc, arcs[0].truth, rung_start(arcs[0].truth, s));
        print_gn(arcs[0].design.name, s, g);
        if (g.cls == GnClass::diverged) ++diverged;
        if (g.cls == GnClass::reached_truth) ++reached;
    }
    std::printf("ARCRUN GN-premise gate-arc diverged_rungs=%zu reached_truth_rungs=%zu of %zu\n", diverged, reached, kLadder.size());
    std::fflush(stdout);
    if (diverged == 0) {
        std::printf("ARCRUN FINDING Gauss-Newton is classed DIVERGED at no rung of the gate arc: stop and report (nothing adjusted, LM not run)\n");
        FAIL("the premise of the control did not hold: GN diverged at no rung (a finding, registered)");
    }
    for (double s : kLadder) print_gn(arcs[1].design.name, s, run_gauss_newton(*arcs[1].arc, arcs[1].truth, rung_start(arcs[1].truth, s)));
    // phase 2: LM, both arcs
    for (std::size_t a = 0; a < arcs.size(); ++a)
        for (double s : kLadder) {
            const auto l = run_lm(*arcs[a].arc, arcs[a].truth, rung_start(arcs[a].truth, s));
            print_lm(arcs[a].design.name, s, l, arcs[a].arc->count());
            if (a == 0) CHECK(l.error.within_gate());          // the registered gate: the noise-free gate arc, every rung
        }
    // phase 3: the one seeded-noise run, recorded only
    {
        ArcModel noisy(forces, arcs[0].design, arcs[0].grid, kArcGridStepS);
        noisy.generate_data(arcs[0].truth, 20251220ULL, kRangeSigmaM);
        std::printf("ARCRUN data arc=gate-noeclipse-noise seed=20251220 data_hash=%s\n", noisy.data_hash().c_str());
        const auto l = run_lm(noisy, arcs[0].truth, rung_start(arcs[0].truth, 10.0));
        print_lm("gate-noeclipse-noise", 10.0, l, noisy.count());
    }
}
