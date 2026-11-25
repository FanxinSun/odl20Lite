// arc_long2_tests.cpp — L7 step 3, part 2: the runs authorised by the manager's ruling of 2026-10-31 (L7_step3_registration.txt, amendment 6), each ONCE.  HIDDEN ([.long]); the exact commands, from the build tree
// (never a tag expression with "~", which is a union):
//     ./tests/odl_l7_arc_tests "ARC-L-002  the sail arc, stages a to d"
//     ./tests/odl_l7_arc_tests "ARC-L-003  the sparse arc"
// Their logs are the record (sha256sum them).  ARC-L-001 (the premise stop) is kept as it ran.
#include "arc_runs.hpp"

using namespace l7arc;

namespace {
struct Prepared {
    ArcDesign design;
    bool eclipsing;
    std::array<double, 6> truth;
    std::vector<odl::frames::GcrsState> grid;
    std::unique_ptr<ArcModel> arc;
};
Prepared prepare(const ArcForces& forces, bool eclipsing, ScheduleRule rule) {
    const ArcDesign d = eclipsing ? eclipsing_arc_design(3.0) : gate_arc_design(3.0);
    Prepared p{d, eclipsing, cartesian_of(d), {}, nullptr};
    p.grid = state_grid(forces, state_from(epoch_of(d), p.truth), d.days * 86400.0, kArcGridStepS);
    p.arc = std::make_unique<ArcModel>(forces, d, p.grid, kArcGridStepS, rule);
    p.arc->generate_data(p.truth, 0, 0.0);
    std::printf("ARCRUN data arc=%s observations=%zu schedule_hash=%s data_hash=%s\n", d.name, p.arc->count(), p.arc->schedule_hash().c_str(), p.arc->data_hash().c_str());
    std::fflush(stdout);
    return p;
}
std::string label(const Prepared& a, LadderKind k) { return std::string(a.design.name) + "/" + to_string(k); }
}  // namespace

TEST_CASE("ARC-L-002  the sail arc, stages a to d", "[.long]") {
    ArcForces forces;
    Prepared gate = prepare(forces, false, ScheduleRule::AllStations);
    Prepared ecl = prepare(forces, true, ScheduleRule::AllStations);
    REQUIRE(gate.arc->schedule_hash() == kGateScheduleHash);
    REQUIRE(gate.arc->data_hash() == kGateDataHash);
    REQUIRE(ecl.arc->schedule_hash() == kEclipsingScheduleHash);
    REQUIRE(ecl.arc->data_hash() == kEclipsingDataHash);
    const LadderKind kinds[] = {LadderKind::P, LadderKind::C};
    // a. the seeded-noise run: GATED (amendment 5); `stalled` stops everything
    {
        ArcModel noisy(forces, gate.design, gate.grid, kArcGridStepS);
        noisy.generate_data(gate.truth, 20251220ULL, kRangeSigmaM);
        std::printf("ARCRUN data arc=gate-noeclipse-noise seed=20251220 data_hash=%s\n", noisy.data_hash().c_str());
        const auto l = run_lm(noisy, gate.truth, start_of(LadderKind::P, gate.design, gate.truth, kRungsPGate[2]));
        print_lm("gate-noeclipse-noise/P", kRungsPGate[2], l, noisy.count());
        if (l.result.status == odl::estimation::LmStatus::stalled) {
            std::printf("ARCRUN FINDING the seeded-noise run ended `stalled`: the stopping rule fails for real-looking data: STOP and report (nothing adjusted)\n");
            std::fflush(stdout);
            FAIL("amendment 5: the noise run ended stalled");
        }
        CHECK(is_converged(l.result.status));
    }
    // b. GN on the eclipsing arc, all ten rungs
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, true)) print_gn(label(ecl, k).c_str(), s, run_gauss_newton(*ecl.arc, ecl.truth, start_of(k, ecl.design, ecl.truth, s)));
    // c. LM on the gate arc, under the registered error gate;  d. LM on the eclipsing arc, recorded
    for (Prepared* a : {&gate, &ecl})
        for (LadderKind k : kinds)
            for (double s : rungs_of(k, a->eclipsing)) {
                const auto l = run_lm(*a->arc, a->truth, start_of(k, a->design, a->truth, s));
                print_lm(label(*a, k).c_str(), s, l, a->arc->count());
                if (!a->eclipsing) CHECK(l.error.within_gate());
            }
}

TEST_CASE("ARC-L-003  the sparse arc", "[.long]") {
    ArcForces forces;
    Prepared sp = prepare(forces, false, ScheduleRule::FirstStationFirstAndLastPass);
    REQUIRE(sp.arc->schedule_hash() == kSparseScheduleHash);
    REQUIRE(sp.arc->data_hash() == kSparseDataHash);
    std::size_t diverged = 0;
    for (double s : kRungsPSparse) {
        const auto g = run_gauss_newton(*sp.arc, sp.truth, start_of(LadderKind::P, sp.design, sp.truth, s));
        print_gn("sparse/P", s, g);
        if (g.cls == GnClass::diverged) ++diverged;
    }
    std::printf("ARCRUN SPARSE-clause GN diverged_rungs=%zu of %zu (%s)\n", diverged, kRungsPSparse.size(), diverged > 0 ? "the step's clause HAS its evidence" : "the clause is RETIRED as evidence; no other arc is sought");
    std::fflush(stdout);
    for (double s : kRungsPSparse) {
        const auto l = run_lm(*sp.arc, sp.truth, start_of(LadderKind::P, sp.design, sp.truth, s));
        print_lm("sparse/P", s, l, sp.arc->count());
        CHECK(l.error.within_gate());
    }
}

// ARC-L-002b — stages b to d of Amendment 9 (L7_step3_registration.txt, 2026-11-25), each ONCE: b = Gauss-Newton on the eclipsing arc, c = LM on the gate arc (gated by the error gate), d = LM on the eclipsing arc (recorded).
// ARC-L-002 above is the historical record of Amendments 5 and 6 and is untouched.  The name contains a comma, which Catch2 reads as a filter separator, and "ARC-L-002*" would also match ARC-L-002 above: the ONE command is
//     ./tests/odl_l7_arc_tests "ARC-L-002b*"
TEST_CASE("ARC-L-002b  the sail arc, stages b to d (Amendment 9)", "[.long]") {
    ArcForces forces;
    Prepared gate = prepare(forces, false, ScheduleRule::AllStations);
    Prepared ecl = prepare(forces, true, ScheduleRule::AllStations);
    REQUIRE(gate.arc->schedule_hash() == kGateScheduleHash);
    REQUIRE(gate.arc->data_hash() == kGateDataHash);
    REQUIRE(ecl.arc->schedule_hash() == kEclipsingScheduleHash);
    REQUIRE(ecl.arc->data_hash() == kEclipsingDataHash);
    const LadderKind kinds[] = {LadderKind::P, LadderKind::C};
    // stage b: Gauss-Newton on the eclipsing arc, all ten frozen rungs
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, true)) print_gn(label(ecl, k).c_str(), s, run_gauss_newton(*ecl.arc, ecl.truth, start_of(k, ecl.design, ecl.truth, s)));
    // stage c: LM on the gate arc, all ten frozen rungs, under the registered error gate
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, false)) {
            const auto l = run_lm(*gate.arc, gate.truth, start_of(k, gate.design, gate.truth, s));
            print_lm(label(gate, k).c_str(), s, l, gate.arc->count());
            CHECK(l.error.within_gate());
        }
    // stage d: LM on the eclipsing arc, all ten frozen rungs, recorded and not gated
    for (LadderKind k : kinds)
        for (double s : rungs_of(k, true)) {
            const auto l = run_lm(*ecl.arc, ecl.truth, start_of(k, ecl.design, ecl.truth, s));
            print_lm(label(ecl, k).c_str(), s, l, ecl.arc->count());
        }
}
