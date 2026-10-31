// arc_long_tests.cpp — L7 step 3, part 2: THE STEP'S EVIDENCE ON THE SAIL ARC, as registered (L7_step3_registration.txt: part 2 and amendments 3 and 4).
//
// HIDDEN ([.long]): it takes hours.  The ONE command that runs it, from the build tree (never a tag expression with "~", which is a union):
//     ./tests/odl_l7_arc_tests "ARC-L-001  the sail arc, as registered"
// and its log is the record (hash it: sha256sum).  Order: every Gauss-Newton rung of the GATE arc, on ladders P and C (five rungs each, frozen); if none is classed DIVERGED the run STOPS here (a
// finding); then the eclipsing arc's Gauss-Newton rungs (recorded); then LM on both arcs, ladder P first, then C; then the one seeded-noise LM run (gate arc, ladder P, rung 3, seed 20251220).
// The gate is the noise-free gate arc only.
#include "arc_runs.hpp"

using namespace l7arc;

TEST_CASE("ARC-L-001  the sail arc, as registered", "[.long]") {
    ArcForces forces;
    struct Prepared {
        ArcDesign design;
        bool eclipsing;
        std::array<double, 6> truth;
        std::vector<odl::frames::GcrsState> grid;
        std::unique_ptr<ArcModel> arc;
    };
    std::vector<Prepared> arcs;
    for (const bool ecl : {false, true}) {
        const ArcDesign d = ecl ? eclipsing_arc_design(3.0) : gate_arc_design(3.0);
        Prepared p{d, ecl, cartesian_of(d), {}, nullptr};
        p.grid = state_grid(forces, state_from(epoch_of(d), p.truth), d.days * 86400.0, kArcGridStepS);
        p.arc = std::make_unique<ArcModel>(forces, d, p.grid, kArcGridStepS);
        p.arc->generate_data(p.truth, 0, 0.0);
        std::printf("ARCRUN data arc=%s observations=%zu schedule_hash=%s data_hash=%s\n", d.name, p.arc->count(), p.arc->schedule_hash().c_str(), p.arc->data_hash().c_str());
        std::fflush(stdout);
        arcs.push_back(std::move(p));
    }
    const LadderKind kinds[] = {LadderKind::P, LadderKind::C};
    auto label = [](const Prepared& a, LadderKind k) { return std::string(a.design.name) + "/" + to_string(k); };
    // every start must evaluate (frozen so; a start that cannot be evaluated is no rung)
    for (const auto& a : arcs)
        for (LadderKind k : kinds)
            for (double s : rungs_of(k, a.eclipsing)) {
                bool ok = a.arc->evaluate(start_of(k, a.design, a.truth, s), false).has_value();
                std::printf("ARCRUN start arc=%s rung=%.3g evaluable=%d\n", label(a, k).c_str(), s, ok ? 1 : 0);
                REQUIRE(ok);
            }
    // phase 1: Gauss-Newton on the gate arc, both ladders
    std::size_t diverged = 0, reached = 0, total = 0;
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, false)) {
            const auto g = run_gauss_newton(*arcs[0].arc, arcs[0].truth, start_of(k, arcs[0].design, arcs[0].truth, s));
            print_gn(label(arcs[0], k).c_str(), s, g);
            ++total;
            if (g.cls == GnClass::diverged) ++diverged;
            if (g.cls == GnClass::reached_truth) ++reached;
        }
    std::printf("ARCRUN GN-premise gate-arc diverged_rungs=%zu reached_truth_rungs=%zu of %zu\n", diverged, reached, total);
    std::fflush(stdout);
    if (diverged == 0) {
        std::printf("ARCRUN FINDING Gauss-Newton is classed DIVERGED at no rung of either ladder on the gate arc: stop and report (nothing adjusted, LM not run)\n");
        FAIL("the premise of the control did not hold: GN diverged at no rung (a finding, registered)");
    }
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, true)) print_gn(label(arcs[1], k).c_str(), s, run_gauss_newton(*arcs[1].arc, arcs[1].truth, start_of(k, arcs[1].design, arcs[1].truth, s)));
    // phase 2: LM, both arcs, ladder P first
    for (std::size_t a = 0; a < arcs.size(); ++a)
        for (LadderKind k : kinds)
            for (double s : rungs_of(k, arcs[a].eclipsing)) {
                const auto l = run_lm(*arcs[a].arc, arcs[a].truth, start_of(k, arcs[a].design, arcs[a].truth, s));
                print_lm(label(arcs[a], k).c_str(), s, l, arcs[a].arc->count());
                if (a == 0) CHECK(l.error.within_gate());          // the registered gate: the noise-free gate arc, every rung
            }
    // phase 3: the one seeded-noise run, recorded only
    {
        ArcModel noisy(forces, arcs[0].design, arcs[0].grid, kArcGridStepS);
        noisy.generate_data(arcs[0].truth, 20251220ULL, kRangeSigmaM);
        std::printf("ARCRUN data arc=gate-noeclipse-noise seed=20251220 data_hash=%s\n", noisy.data_hash().c_str());
        const auto l = run_lm(noisy, arcs[0].truth, start_of(LadderKind::P, arcs[0].design, arcs[0].truth, kRungsPGate[2]));
        print_lm("gate-noeclipse-noise/P", kRungsPGate[2], l, noisy.count());
        // amendment 5: GATED -- the stopping rule must end converged_* on real-looking noise (stalled here = the rule fails for real data: stop and report)
        const auto st = l.result.status;
        CHECK((st == odl::estimation::LmStatus::converged_gradient || st == odl::estimation::LmStatus::converged_residual || st == odl::estimation::LmStatus::converged_step));
    }
}
