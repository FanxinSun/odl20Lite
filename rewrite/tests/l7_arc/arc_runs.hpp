#pragma once
// arc_runs.hpp — L7 step 3, part 2: the runs on the arc, as registered (L7_step3_registration.txt, part 2 and its amendment 3).
#include "arc_model.hpp"

#include "../../modules/estimation/tests/lm_controls.hpp"

#include <chrono>
#include <cstdio>

namespace l7arc {

inline constexpr double kGatePositionM = 1.0, kGateVelocityMps = 1.0e-3;    // the gate: within 1 m and 1 mm/s of the truth at the first epoch
inline constexpr double kDivergenceFactor = 1.0e6;                          // amendment 3: F > 1e6 F(x0) is "diverged"

inline double wall_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct TruthError {
    double position_m = 0.0, velocity_mps = 0.0;
    [[nodiscard]] bool within_gate() const { return position_m <= kGatePositionM && velocity_mps <= kGateVelocityMps; }
};
inline TruthError error_at_truth(std::span<const double> x, const std::array<double, 6>& truth) {
    TruthError e;
    for (std::size_t i = 0; i < 3; ++i) { e.position_m += (x[i] - truth[i]) * (x[i] - truth[i]); e.velocity_mps += (x[i + 3] - truth[i + 3]) * (x[i + 3] - truth[i + 3]); }
    e.position_m = std::sqrt(e.position_m);
    e.velocity_mps = std::sqrt(e.velocity_mps);
    return e;
}

inline const std::vector<std::string>& parameter_names() {
    static const std::vector<std::string> n{"x0_m", "y0_m", "z0_m", "vx0_m_s", "vy0_m_s", "vz0_m_s"};
    return n;
}

enum class GnClass { reached_truth, diverged, wrong_point, not_settled, solver_refused };
inline const char* to_string(GnClass c) {
    switch (c) {
        case GnClass::reached_truth: return "reached-truth";
        case GnClass::diverged: return "DIVERGED";
        case GnClass::wrong_point: return "wrong-point";
        case GnClass::not_settled: return "not-settled";
        case GnClass::solver_refused: return "solver-refused";
    }
    return "?";
}

struct GnOutcome {
    GnClass cls = GnClass::not_settled;
    std::string why;
    std::size_t iterations = 0;
    double f0 = 0.0, f_final = 0.0;
    TruthError error;
    bool within_gate_at_kmax = false;      // a "not settled" run that is nevertheless inside the gate (the step test cannot fire under the integrator's noise): reported, not reclassified
    double seconds = 0.0;
};

/// Gauss-Newton once from `x0`, classed by amendment 3.  The model is wrapped so that a run is STOPPED the moment F exceeds kDivergenceFactor x F(x0).
inline GnOutcome run_gauss_newton(const ArcModel& arc, const std::array<double, 6>& truth, const std::array<double, 6>& x0) {
    GnOutcome out;
    const double w0 = wall_s();
    bool model_refused = false, exploded = false;
    std::size_t evals = 0, refused_at_eval = 0;
    std::string refusal_id;
    double f0 = -1.0;
    const odl::estimation::LmModel inner = lm_model_of(arc, kRangeSigmaM);
    const odl::estimation::LmModel wrapped = [&](std::span<const double> x, bool jac) -> odl::Result<odl::estimation::LmEvaluation, odl::estimation::EstimationRefusal> {
        auto e = inner(x, jac);
        ++evals;
        if (!e) { model_refused = true; refused_at_eval = evals; refusal_id = e.error().id; return e; }
        double s = 0.0;
        for (double r : e->residual) s += r * r;
        const double f = 0.5 * s;
        if (!std::isfinite(f)) { exploded = true; refused_at_eval = evals; return odl::err(odl::estimation::EstimationRefusal{"ARC-F-002", "non-finite objective"}); }
        if (f0 < 0.0) f0 = f;
        if (f > kDivergenceFactor * f0) { exploded = true; refused_at_eval = evals; return odl::err(odl::estimation::EstimationRefusal{"ARC-F-003", "objective above 1e6 F(x0)"}); }
        std::printf("ARCGN iter_eval F=%.6e\n", f);
        std::fflush(stdout);
        return e;
    };
    const auto run = odl::estimation::fx::gauss_newton(parameter_names(), x0, wrapped, 200);
    out.seconds = wall_s() - w0;
    out.iterations = run.iterations;
    out.f0 = f0;
    out.f_final = run.objective;
    out.error = error_at_truth(run.x, truth);
    if (exploded) { out.cls = GnClass::diverged; out.why = "numeric divergence at step " + std::to_string(refused_at_eval - 1) + " (non-finite, or F above 1e6 F(x0))"; }
    else if (model_refused) { out.cls = GnClass::diverged; out.why = "left the model domain at step " + std::to_string(refused_at_eval - 1) + " (" + refusal_id + ")"; }
    else if (run.ended == "non-finite") { out.cls = GnClass::diverged; out.why = "numeric divergence (non-finite iterate)"; }
    else if (run.ended == "refused") { out.cls = GnClass::solver_refused; out.why = "step 2 refused the normal system"; }
    else if (run.ended == "step") { out.cls = out.error.within_gate() ? GnClass::reached_truth : GnClass::wrong_point; out.why = "step test"; }
    else { out.cls = GnClass::not_settled; out.why = "200 steps"; out.within_gate_at_kmax = out.error.within_gate(); }
    return out;
}

struct LmOutcome {
    odl::estimation::LmResult result;
    TruthError error;
    std::size_t evaluations = 0;
    double seconds = 0.0;
};

inline LmOutcome run_lm(const ArcModel& arc, const std::array<double, 6>& truth, const std::array<double, 6>& x0, const odl::estimation::LmOptions& options = {}) {
    LmOutcome out;
    const double w0 = wall_s();
    const auto model = lm_model_of(arc, kRangeSigmaM, &out.evaluations);
    out.result = odl::estimation::levenberg_marquardt(parameter_names(), x0, model, options);
    out.seconds = wall_s() - w0;
    out.error = error_at_truth(out.result.x, truth);
    return out;
}

inline void print_gn(const char* arc, double s, const GnOutcome& g) {
    std::printf("ARCRUN GN arc=%s rung=%.3g class=%s why=\"%s\" iters=%zu F0=%.6e F=%.6e dr_m=%.6e dv_mps=%.6e inside_gate_at_200=%d wall_s=%.0f\n", arc, s, to_string(g.cls), g.why.c_str(),
                g.iterations, g.f0, g.f_final, g.error.position_m, g.error.velocity_mps, g.within_gate_at_kmax ? 1 : 0, g.seconds);
    std::fflush(stdout);
}
inline void print_lm(const char* arc, double s, const LmOutcome& l, std::size_t m) {
    std::printf("ARCRUN LM arc=%s rung=%.3g status=%s iters=%zu accepted=%zu rejected=%zu F=%.6e chi2_per_obs=%.4f cosine=%.3e dr_m=%.6e dv_mps=%.6e gate=%s evals=%zu wall_s=%.0f msg=\"%s\"\n", arc, s,
                odl::estimation::to_string(l.result.status), l.result.iterations, l.result.accepted_steps, l.result.rejected_steps, l.result.objective, 2.0 * l.result.objective / static_cast<double>(m),
                l.result.cosine, l.error.position_m, l.error.velocity_mps, l.error.within_gate() ? "PASS" : "FAIL", l.evaluations, l.seconds, l.result.message.c_str());
    std::fflush(stdout);
}

}  // namespace l7arc

namespace l7arc {
inline constexpr double kArcGridStepS = 60.0;
/// The two registered designs (part 2): epoch 2025-12-20 00:00 UTC; the gate arc is the sun-synchronous dawn-dusk plane with RAAN -2.0 deg (frozen), the eclipsing arc is i = 24 deg, RAAN 100 deg.
inline ArcDesign gate_arc_design(double days) { return ArcDesign{"gate-noeclipse", 2025, 12, 20, 0, 6378137.0 + 720e3, 0.001, 98.27, -2.0, 90.0, 0.0, days, 60.0}; }
inline ArcDesign eclipsing_arc_design(double days) { return ArcDesign{"eclipsing", 2025, 12, 20, 0, 6378137.0 + 720e3, 0.001, 24.0, 100.0, 0.0, 0.0, days, 60.0}; }
}  // namespace l7arc

namespace l7arc {
/// The two ladders of the ruling of 2026-10-31 (L7-3.md), both a scalar s times a fixed error vector at the first epoch:
///  P (the realistic one): ALONG-TRACK PHASE and SEMI-MAJOR AXIS only, the rest of the state at truth.  Position shifted by s km along the velocity direction; speed changed along the velocity
///      by the vis-viva change of a semi-major axis error of s x 10 m:  dv = |v| da / (2 a).
///  C (continuity): the old Cartesian direction ((0.5,-0.7,0.5), (-0.6,0.3,0.74), each normalised), scaled to s km and s cm/s -- used with s below the old unit.
enum class LadderKind { P, C };
inline std::array<double, 6> start_of(LadderKind kind, const ArcDesign& d, const std::array<double, 6>& truth, double s) {
    if (kind == LadderKind::C) return rung_start(truth, s);
    const Vec3 v{truth[3], truth[4], truth[5]};
    const double speed = v.norm();
    const Vec3 vh = (1.0 / speed) * v;
    const double dL = 1000.0 * s, da = 10.0 * s, dv = speed * da / (2.0 * d.a_m);
    return {truth[0] + dL * vh.x, truth[1] + dL * vh.y, truth[2] + dL * vh.z, truth[3] + dv * vh.x, truth[4] + dv * vh.y, truth[5] + dv * vh.z};
}
inline const char* to_string(LadderKind k) { return k == LadderKind::P ? "P" : "C"; }
}  // namespace l7arc

namespace l7arc {
/// The FROZEN rungs (L7_step3_registration.txt, amendment 4 results).
inline constexpr std::array<double, 5> kRungsPGate{1.0, 2.9, 8.8, 26.0, 79.0};
inline constexpr std::array<double, 5> kRungsPEclipsing{1.0, 2.9, 8.7, 25.0, 76.0};
inline constexpr std::array<double, 5> kRungsC{0.01, 0.034, 0.12, 0.42, 1.5};          // both arcs
inline const std::array<double, 5>& rungs_of(LadderKind kind, bool eclipsing) { return kind == LadderKind::C ? kRungsC : (eclipsing ? kRungsPEclipsing : kRungsPGate); }
}  // namespace l7arc

namespace l7arc {
inline constexpr std::array<double, 5> kRungsPSparse{1.0, 3.0, 9.2, 28.0, 86.0};     // the sparse arc's frozen ladder P (amendment 6)
// the dense arcs' hashes, frozen at part 2 (asserted by the long cases: the fixture changed since, and must not have changed them)
inline constexpr const char* kGateScheduleHash = "59d3bd869e56fcdf";
inline constexpr const char* kGateDataHash = "c6394e3463de9e69";
inline constexpr const char* kEclipsingScheduleHash = "36002f4410e37526";
inline constexpr const char* kEclipsingDataHash = "9d17c912d5a228f2";
inline constexpr const char* kSparseScheduleHash = "a014e539b20b975c";
inline constexpr const char* kSparseDataHash = "04e81374e08fa9a8";
inline bool is_converged(odl::estimation::LmStatus st) {
    return st == odl::estimation::LmStatus::converged_gradient || st == odl::estimation::LmStatus::converged_residual || st == odl::estimation::LmStatus::converged_step;
}
}  // namespace l7arc
