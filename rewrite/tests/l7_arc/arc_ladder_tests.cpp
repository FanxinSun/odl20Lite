// arc_ladder_tests.cpp — L7 step 3, part 2: the physics of the drift (ARC-X-005) and the evaluable edge of each ladder (ARC-X-006), by MODEL CALLS ONLY: no fit, no gradient.
// Hidden ([.geometry]).  Registered in L7_step3_registration.txt (amendment 4) before they ran.
#include "arc_runs.hpp"

using namespace l7arc;

namespace {
double rms_residual_m(const ArcModel& arc, const std::array<double, 6>& x, bool* ok, std::string* why = nullptr) {
    auto ev = arc.evaluate(x, false);
    if (!ev) { *ok = false; if (why) *why = ev.error().message.substr(0, 110); return 0.0; }
    *ok = true;
    double s = 0.0;
    for (double r : ev->residual_m) s += r * r;
    return std::sqrt(s / static_cast<double>(arc.count()));
}
double two_figures(double x) {
    const double e = std::pow(10.0, std::floor(std::log10(x)) - 1.0);
    return std::floor(x / e) * e;       // rounded DOWN to two figures
}
}  // namespace

TEST_CASE("ARC-X-005  where the drift comes from: single-direction starts of the gate arc", "[.geometry]") {
    ArcForces forces;
    const ArcDesign d = gate_arc_design(3.0);
    const auto truth = cartesian_of(d);
    const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
    ArcModel arc(forces, d, grid, kArcGridStepS);
    arc.generate_data(truth, 0, 0.0);
    const Vec3 r{truth[0], truth[1], truth[2]}, v{truth[3], truth[4], truth[5]};
    const Vec3 rh = (1.0 / r.norm()) * r, vh = (1.0 / v.norm()) * v;
    const Vec3 dr = (1.0 / std::sqrt(0.5 * 0.5 + 0.7 * 0.7 + 0.5 * 0.5)) * Vec3{0.5, -0.7, 0.5};
    const Vec3 dv = (1.0 / std::sqrt(0.6 * 0.6 + 0.3 * 0.3 + 0.74 * 0.74)) * Vec3{-0.6, 0.3, 0.74};
    auto dot = [](const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    std::printf("ARCPHYS old_dr: radial_fraction=%.4f along_track_fraction=%.4f | old_dv: radial=%.4f along_track=%.4f\n", dot(dr, rh), dot(dr, vh), dot(dv, rh), dot(dv, vh));
    std::printf("ARCPHYS predicted (manager): 1 cm/s along track -> da=19 m, drift 8 km; 1 km radial -> da=2 km, drift ~800 km\n");
    struct Case { const char* name; std::array<double, 6> x; };
    auto with = [&](Vec3 dpos, Vec3 dvel) { return std::array<double, 6>{truth[0] + dpos.x, truth[1] + dpos.y, truth[2] + dpos.z, truth[3] + dvel.x, truth[4] + dvel.y, truth[5] + dvel.z}; };
    const Case cases[] = {
        {"old dr only (1 km)", with(1000.0 * dr, Vec3{0, 0, 0})},
        {"old dv only (1 cm/s)", with(Vec3{0, 0, 0}, 0.01 * dv)},
        {"radial position 1 km", with(1000.0 * rh, Vec3{0, 0, 0})},
        {"along-track position 1 km", with(1000.0 * vh, Vec3{0, 0, 0})},
        {"along-track velocity 1 cm/s", with(Vec3{0, 0, 0}, 0.01 * vh)},
        {"radial velocity 1 cm/s", with(Vec3{0, 0, 0}, 0.01 * rh)},
        {"ladder P at s=1 (1 km along-track, da=10 m)", start_of(LadderKind::P, d, truth, 1.0)},
    };
    for (const auto& c : cases) {
        bool ok = false;
        std::string why;
        const double rms = rms_residual_m(arc, c.x, &ok, &why);
        std::printf("ARCPHYS %-46s %s rms_range_residual_m=%.4e %s\n", c.name, ok ? "ok     " : "REFUSED", rms, why.c_str());
        std::fflush(stdout);
    }
}

TEST_CASE("ARC-X-006  the evaluable edge of each ladder on each arc, and the rungs", "[.geometry]") {
    ArcForces forces;
    for (const ArcDesign& d : {gate_arc_design(3.0), eclipsing_arc_design(3.0)}) {
        const auto truth = cartesian_of(d);
        const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
        ArcModel arc(forces, d, grid, kArcGridStepS);
        arc.generate_data(truth, 0, 0.0);
        for (LadderKind kind : {LadderKind::P, LadderKind::C}) {
            const double s0 = kind == LadderKind::P ? 1.0 : 0.01;
            auto ok_at = [&](double s) { bool ok = false; (void)rms_residual_m(arc, start_of(kind, d, truth, s), &ok); return ok; };
            // doubling from the lowest rung until the first refusal (or 1e4), then bisection on log s until hi/lo <= 1.04
            double lo = s0, hi = s0;
            if (!ok_at(s0)) { std::printf("ARCEDGE arc=%s ladder=%s lowest rung s=%.3g NOT EVALUABLE\n", d.name, to_string(kind), s0); continue; }
            while (hi < 1e4) { hi *= 2.0; if (!ok_at(hi)) break; lo = hi; }
            if (hi >= 1e4 && ok_at(hi)) { std::printf("ARCEDGE arc=%s ladder=%s evaluable up to the search limit 1e4\n", d.name, to_string(kind)); continue; }
            while (hi / lo > 1.04) { const double mid = std::sqrt(lo * hi); if (ok_at(mid)) lo = mid; else hi = mid; }
            const double smax = two_figures(lo);
            std::printf("ARCEDGE arc=%s ladder=%s last_evaluable=%.4g first_refused=%.4g s_max(2 figures, rounded down)=%.3g\n", d.name, to_string(kind), lo, hi, smax);
            std::fflush(stdout);
            // five geometric rungs from the lowest to s_max, each rounded to two figures (the last is s_max itself), each checked and its F printed
            for (int i = 0; i < 5; ++i) {
                double s = i == 4 ? smax : two_figures(s0 * std::pow(smax / s0, i / 4.0));
                if (i == 0) s = s0;
                auto ev = arc.evaluate(start_of(kind, d, truth, s), false);
                if (!ev) { std::printf("ARCEDGE arc=%s ladder=%s rung=%d s=%.3g REFUSED\n", d.name, to_string(kind), i + 1, s); continue; }
                double f = 0.0;
                for (double r : ev->residual_m) f += 0.5 * (r / kRangeSigmaM) * (r / kRangeSigmaM);
                std::printf("ARCEDGE arc=%s ladder=%s rung=%d s=%.3g F=%.6e\n", d.name, to_string(kind), i + 1, s, f);
                std::fflush(stdout);
            }
        }
    }
}

// ARC-X-007 — the SPARSE ARC's preparation (amendment 6): schedule by the rule, data and hashes, the Jacobian's conditioning at the truth, and the evaluable edge of ladder P with its five rungs.  Model calls only.  No fit.
TEST_CASE("ARC-X-007  the sparse arc: schedule, hashes, conditioning at the truth, edge and rungs", "[.geometry]") {
    ArcForces forces;
    const ArcDesign d = gate_arc_design(3.0);
    const auto truth = cartesian_of(d);
    const auto grid = state_grid(forces, state_from(epoch_of(d), truth), d.days * 86400.0, kArcGridStepS);
    ArcModel arc(forces, d, grid, kArcGridStepS, ScheduleRule::FirstStationFirstAndLastPass);
    arc.generate_data(truth, 0, 0.0);
    std::printf("ARCSPARSE observations=%zu epochs=%zu first_pass_nodes=%zu last_pass_nodes=%zu same_pass=%d schedule_hash=%s data_hash=%s\n", arc.count(), arc.epochs().size(), arc.first_pass_nodes(), arc.last_pass_nodes(),
                arc.first_and_last_pass_are_one() ? 1 : 0, arc.schedule_hash().c_str(), arc.data_hash().c_str());
    std::fflush(stdout);
    REQUIRE(arc.count() > 0);
    // conditioning at the truth
    auto at_truth = arc.evaluate(truth, true);
    REQUIRE(at_truth.has_value());
    {
        const std::size_t m = arc.count();
        std::vector<double> n(36, 0.0), rhs(6, 0.0);
        for (std::size_t k = 0; k < m; ++k)
            for (std::size_t i = 0; i < 6; ++i)
                for (std::size_t j = 0; j < 6; ++j) n[i * 6 + j] += (at_truth->rows[k * 6 + i] / kRangeSigmaM) * (at_truth->rows[k * 6 + j] / kRangeSigmaM);
        auto ne = odl::estimation::NormalEquations::from_normal_system(parameter_names(), n, rhs, m, 0.0);
        if (!ne) { std::printf("ARCSPARSE conditioning REFUSED at construction: %s %s\n", ne.error().id.data(), ne.error().message.c_str()); std::fflush(stdout); FAIL("rank-deficient (refused at construction): STOP and tell the manager"); }
        auto sol = ne->solve();
        if (!sol) {
            std::printf("ARCSPARSE conditioning REFUSED: %s %s (kappa_est %.3e kappa_up %.3e)\n", sol.error().id.data(), sol.error().message.c_str(), sol.error().condition.condition_estimate, sol.error().condition.condition_upper);
            std::fflush(stdout);
            FAIL("rank-deficient: STOP and tell the manager");
        }
        std::printf("ARCSPARSE conditioning at truth: kappa_est=%.6e kappa_up=%.6e sqrt(kappa_est)=%.6e B=%.3e\n", sol->condition.condition_estimate, sol->condition.condition_upper, std::sqrt(sol->condition.condition_estimate),
                    sol->certified_relative_error);
        for (std::size_t i = 0; i < 6; ++i) std::printf("ARCSPARSE parameter_condition %s = %.4e\n", parameter_names()[i].c_str(), sol->condition.parameter_conditions[i]);
        std::fflush(stdout);
    }
    // the edge of ladder P and its five rungs
    auto ok_at = [&](double s) { bool ok = false; (void)rms_residual_m(arc, start_of(LadderKind::P, d, truth, s), &ok); return ok; };
    double lo = 1.0, hi = 1.0;
    REQUIRE(ok_at(1.0));
    while (hi < 1e4) { hi *= 2.0; if (!ok_at(hi)) break; lo = hi; }
    if (hi >= 1e4 && ok_at(hi)) { std::printf("ARCSPARSE edge: evaluable up to the search limit 1e4: STOP for the manager\n"); std::fflush(stdout); FAIL("no refusal up to 1e4"); }
    while (hi / lo > 1.04) { const double mid = std::sqrt(lo * hi); if (ok_at(mid)) lo = mid; else hi = mid; }
    const double smax = two_figures(lo);
    std::printf("ARCSPARSE edge: last_evaluable=%.4g first_refused=%.4g s_max=%.3g\n", lo, hi, smax);
    for (int i = 0; i < 5; ++i) {
        double s = i == 4 ? smax : two_figures(std::pow(smax, i / 4.0));
        if (i == 0) s = 1.0;
        auto ev = arc.evaluate(start_of(LadderKind::P, d, truth, s), false);
        if (!ev) { std::printf("ARCSPARSE rung=%d s=%.3g REFUSED\n", i + 1, s); continue; }
        double f = 0.0;
        for (double r : ev->residual_m) f += 0.5 * (r / kRangeSigmaM) * (r / kRangeSigmaM);
        std::printf("ARCSPARSE rung=%d s=%.3g F=%.6e\n", i + 1, s, f);
        std::fflush(stdout);
    }
}
