// exit_gate_tests.cpp — SPEC-estimation.md EST-A-900, EST-A-900b, EST-A-901, EST-A-902: the L7 exit gate (registered in v0.7, 2026-11-27, commit ed5462c; the sizing frozen in exit_gate_sizing_frozen.hpp, run 1 of
// tools/exit_gate_fd_sizing, SHA-256 db43ee56c60edb190750de366bffd6d069d085f10b8a6b761fca4debd3c68054; the Committee's rulings of TREE-14 recorded in v0.7a).
//
// WRITTEN BEFORE ANY FIT WAS RUN (plan §4 rule 3).  Each case is run ONCE, ALONE (`odl_l7_exit_tests "EST-A-90x*"`, `--list-tests` first), and a registered criterion that misses is REPORTED, never tuned.
// Choices of the executor where the registration is silent are marked "executor-classified" at the place and listed in PROVENANCE.md 40.12.
#include "exit_gate_model.hpp"
#include "exit_gate_sizing_frozen.hpp"

#include <odl/estimation/joint_covariance.hpp>

#include <cstdio>
#include <limits>
#include <numbers>

namespace odl { inline constexpr double kGmFixtureForExit = 3.986004415e14; }   // EGM2008's TT-compatible GM, the figure of the force-model fixtures (used for the printed period only)

using namespace l7exit;

namespace {

constexpr std::size_t kFree7[] = {0, 1, 2, 3, 4, 5, 6};

struct Fit {
    est::LmResult lm;
    Theta start{}, hat{};
    Evaluation at_hat;
    double sn_start = 0, sn_end = 0;
};

Fit run_fit(const Model& m, const Theta& th0, const std::vector<std::size_t>& free, const std::vector<std::string>& nm) {
    Fit f;
    f.start = th0;
    Evaluation e0;
    REQUIRE_FALSE(m.evaluate(th0, false, e0).has_value());
    f.sn_start = s_n(e0.r);
    std::vector<double> x0;
    for (auto i : free) x0.push_back(th0[i]);
    est::LmOptions opt;
    opt.max_iterations = 100;      // executor-classified: a safety limit on the cost; the registered status set refuses max_iterations, so reaching it is a finding
    f.lm = est::levenberg_marquardt(nm, x0, lm_model(m, th0, free), opt);
    f.hat = th0;
    for (std::size_t i = 0; i < free.size(); ++i) f.hat[free[i]] = f.lm.x[i];
    REQUIRE_FALSE(m.evaluate(f.hat, true, f.at_hat).has_value());
    f.sn_end = s_n(f.at_hat.r);
    return f;
}
std::vector<std::string> all_names() { return {names().begin(), names().end()}; }
Fit fit7(const Model& m, const Theta& th0) { return run_fit(m, th0, {0, 1, 2, 3, 4, 5, 6}, all_names()); }

const char* outcome_of(double sn, double snm7, double F) {
    if (snm7 <= F) return "reaches (S_{N-7} <= F: under either denominator)";
    if (sn <= F) return "reaches only under the N reading (S_N <= F < S_{N-7}): brought to the manager unchanged";
    return "does not reach (F < S_N): a finding, brought to the manager unchanged";
}

double shadow_min(const Model& m, const Fit& f) {
    const World& w = world();
    double mn = 1.0;
    for (std::size_t k = 0; k < f.at_hat.states.size(); ++k) {
        auto sun = w.ephemeris.geocentric_state(odl::eph::Body::Sun, m.d.t[k], w.leaps);
        REQUIRE(sun.has_value());
        auto nu = odl::shadow::conical(odl::frames::Position<odl::frames::Frame::GCRS>{odl::metres_from_km(sun->position())}, odl::frames::Position<odl::frames::Frame::GCRS>{odl::metres_from_km(f.at_hat.states[k].position())});
        REQUIRE(nu.has_value());
        mn = std::min(mn, nu->fraction);
    }
    return mn;
}

/// EST-R-902 .. -904: the configuration, the statistic block and the iteration block of one fit, printed.
void print_fit(const Row& r, const char* label, const Model& m, const Fit& f) {
    const double F = r.F, snn = f.sn_end, snm7 = snn * std::sqrt(static_cast<double>(kEpochs) / (kEpochs - 7));
    std::printf("---- %s %s ----\n", r.name, label);
    std::printf("  configuration: %s %s, file %s; model: Gravity EGM2008 degree %d order %d extrapolate_secular=true (the shift's size is not exposed by the Gravity API: only the flag is recorded); third bodies %s; Tides solid + FES2004 degree 7 + solid pole%s; relativity Schwarzschild + Lense-Thirring + de Sitter; SRP %s through ScaledForce; integrator tolerance %.3g; state at epoch %d (43200 s), chained propagation\n",
                r.name, r.sat, r.file, m.cfg.degree, m.cfg.degree, m.cfg.set_b ? "Sun, Moon + five planets (set B)" : "Sun, Moon (set A)", m.cfg.desai ? " + Desai ocean pole tide" : "", m.cfg.box_wing ? (r.iif ? "box-wing gps_block_iif()" : "box-wing gps_block_iir_m()") : "cannonball", kTolerance, kMid);
    if (!m.cfg.box_wing) std::printf("  cannonball: area %.17g m^2, mass %.17g kg, C_R %.17g, A C_R/m = %.17g m^2/kg\n", m.area_m2, m.mass_kg, kCr, m.a_cr_over_m());
    // statistics at the fit
    double sum = 0, mx = 0, rr = 0, tt = 0, nn = 0;
    for (std::size_t k = 0; k < static_cast<std::size_t>(kEpochs); ++k) {
        const Vec3 e_itrs{-f.at_hat.r[3 * k], -f.at_hat.r[3 * k + 1], -f.at_hat.r[3 * k + 2]};     // q - p
        const double n2 = e_itrs.dot(e_itrs);
        sum += n2;
        mx = std::max(mx, std::sqrt(n2));
        const Vec3 e = f.at_hat.to_itrs[k].transpose().apply(e_itrs);                                // into the GCRS
        const Vec3 pos = odl::metres_from_km(f.at_hat.states[k].position()), vel = odl::metres_from_km(f.at_hat.states[k].velocity());
        const Vec3 R = (1.0 / pos.norm()) * pos, Nn = (1.0 / pos.cross(vel).norm()) * pos.cross(vel), T = Nn.cross(R);
        rr += e.dot(R) * e.dot(R); tt += e.dot(T) * e.dot(T); nn += e.dot(Nn) * e.dot(Nn);
    }
    std::printf("  statistics: N = %d; S_N = %.9g m; S_{N-7} = %.9g m; S_N/sqrt(3) = %.9g m; F = %.4f m; S_N/F = %.6g (predicted 0.6 <= S_N/F <= 1.1: %s); RMS radial %.9g, along-track %.9g, cross-track %.9g m; largest epoch residual %.9g m\n",
                kEpochs, snn, snm7, snn / std::sqrt(3.0), F, snn / F, (snn / F >= 0.6 && snn / F <= 1.1) ? "met" : "NOT met", std::sqrt(rr / kEpochs), std::sqrt(tt / kEpochs), std::sqrt(nn / kEpochs), mx);
    std::printf("  outcome row of 8.1's amendment: %s\n", outcome_of(snn, snm7, F));
    std::printf("  parameters (x y z m; vx vy vz m/s; srp_scale):");
    for (std::size_t i = 0; i < kNp; ++i) std::printf(" %.12g", f.hat[i]);
    std::printf("\n");
    if (f.lm.final_system) {
        auto layout = est::JointParameterLayout::create({"x", "y", "z", "vx", "vy", "vz"}, {"srp_scale"});
        REQUIRE(layout.has_value());
        auto jc = est::joint_covariance(*f.lm.final_system, *layout);
        if (jc) {
            std::printf("  standard deviations (formal | scaled by sigma-hat_0):");
            for (std::size_t i = 0; i < kNp; ++i) std::printf(" %s %.6g | %s;", names()[i].c_str(), std::sqrt(jc->formal[i * kNp + i]), jc->scaled ? std::to_string(std::sqrt((*jc->scaled)[i * kNp + i])).c_str() : "n/a");
            std::printf("\n");
        } else std::printf("  joint covariance refused: %s\n", std::string(jc.error().message).c_str());
        std::printf("  condition_estimate = %.9g, degrees of freedom nu = %td\n", f.lm.final_system->condition.condition_estimate, f.lm.final_system->degrees_of_freedom);
    } else std::printf("  no final system (the undamped solve was refused)\n");
    if (!m.cfg.box_wing) std::printf("  c = %.12g; c * A C_R / m = %.12g m^2/kg\n", f.hat[6], f.hat[6] * m.a_cr_over_m());
    std::printf("  iteration: status %s (%s); passes %zu (accepted %zu, rejected %zu); objective F = %.9g, gradient cosine %.3g; S_N(start) = %.9g m, S_N(end) = %.9g m, ratio %.6g (gross-error control >= 10: %s)\n",
                est::to_string(f.lm.status), f.lm.message.c_str(), f.lm.iterations, f.lm.accepted_steps, f.lm.rejected_steps, f.lm.objective, f.lm.cosine, f.sn_start, f.sn_end, f.sn_start / f.sn_end, f.sn_start / f.sn_end >= 10.0 ? "met" : "NOT met");
    const Vec3 p = odl::metres_from_km(f.at_hat.states[static_cast<std::size_t>(kMid)].position()), v = odl::metres_from_km(f.at_hat.states[static_cast<std::size_t>(kMid)].velocity());
    const double a = 1.0 / (2.0 / p.norm() - v.dot(v) / odl::kGmFixtureForExit), T = 2.0 * std::numbers::pi * std::sqrt(a * a * a / odl::kGmFixtureForExit);
    std::printf("  first-step cap note: span/100 = %.6g s against 0.21 T = %.6g s (a = %.9g m, T = %.9g s): the cap %s\n", 43200.0 / 100.0, 0.21 * T, a, T, 0.21 * T > 432.0 ? "does not bind" : "binds");
    std::printf("  shadow: minimum nu over the 96 epochs of the fitted trajectory = %.17g\n", shadow_min(m, f));
    std::fflush(stdout);
}

const odl::l7exit::FrozenSizing& frozen_of(const Row& r) {
    for (const auto& z : odl::l7exit::kFrozen)
        if (std::string(z.row) == r.name) return z;
    FAIL("no frozen sizing for " << r.name);
    return odl::l7exit::kFrozen[0];
}

}  // namespace


// ================================================================================================================================================================================================
TEST_CASE("EST-A-900 the exit gate's fit statistic", "[est][exit][900]") {
    for (const Row& r : rows()) {
        const Data d = read_row(r);
        const Theta th0 = nominal_start(d);
        for (bool box : {false, true}) {
            Config c;
            c.box_wing = box;
            const Model m(r, d, c);
            const Fit f = fit7(m, th0);
            print_fit(r, box ? "box-wing fit" : "cannonball fit", m, f);
            const double snm7 = f.sn_end * std::sqrt(static_cast<double>(kEpochs) / (kEpochs - 7));
            std::printf("  EST-A-900 %s %s: S_N = %.9g, S_{N-7} = %.9g, F = %.4f -> %s\n", r.name, box ? "box-wing" : "cannonball", f.sn_end, snm7, r.F, outcome_of(f.sn_end, snm7, r.F));
            {   // Amendment 1 (recorded beside the first text, which is kept: S_N <= F is printed above, no longer asserted for every fit)
                static const double minima[3][2] = {{0.0622784085, 0.0947894259}, {0.0696950657, 0.107004257}, {0.242196639, 0.241612829}};
                const double mn = minima[&r - rows().data()][box ? 1 : 0];
                std::printf("  Amendment 1: S_N <= 2F: %s; reproduces the recorded minimum %.10g to 1e-6 relative: |dS_N|/S_N = %.3g\n", f.sn_end <= 2.0 * r.F ? "met" : "NOT met", mn, std::fabs(f.sn_end - mn) / mn);
                CHECK(f.sn_end <= 2.0 * r.F);
                CHECK(std::fabs(f.sn_end - mn) <= 1e-6 * mn);
                if (!box && std::string(r.name) == "G-01") CHECK(f.sn_end <= r.F);
            }
        }
    }
}

TEST_CASE("EST-A-900b the exit gate's fits and what each says about itself", "[est][exit][900b]") {
    // (a) the data, whole; the controls (the reader's text edited in memory: the test's scratch copy)
    for (const Row& r : rows()) {
        Data d;
        const std::string text = slurp(r.file);
        const auto ok = read_whole(text, r.sat, d);
        CHECK_FALSE(ok.has_value());
        CHECK(d.t.size() == static_cast<std::size_t>(kEpochs));
        // control 1: one record of the satellite set to 999999.999999
        {
            const std::string key = std::string("\nP") + r.sat;
            std::string t2 = text;
            std::size_t pos = 0;
            for (int i = 0; i < 20; ++i) pos = t2.find(key, pos + 1);       // the 20th record of the satellite: epoch index 19
            REQUIRE(pos != std::string::npos);
            t2.replace(pos + 5, 14, " 999999.999999");
            Data bad;
            const auto e = read_whole(t2, r.sat, bad);
            std::printf("  EST-F-901 control 1 (%s, record of epoch 19 set to 999999.999999): %s\n", r.name, e ? e->c_str() : "NOT REFUSED");
            REQUIRE(e.has_value());
            CHECK(e->find("EST-F-901") == 0);
            CHECK(e->find("19") != std::string::npos);
        }
        // control 2: one epoch line removed (the 30th epoch header)
        {
            std::string t2 = text;
            std::size_t pos = 0;
            for (int i = 0; i < 30; ++i) pos = t2.find("\n*  ", pos + 1);
            REQUIRE(pos != std::string::npos);
            const std::size_t end = t2.find('\n', pos + 1);
            t2.erase(pos, end - pos);
            Data bad;
            const auto e = read_whole(t2, r.sat, bad);
            std::printf("  EST-F-901 control 2 (%s, the epoch line of epoch 29 removed): %s\n", r.name, e ? e->c_str() : "NOT REFUSED");
            REQUIRE(e.has_value());
            CHECK(e->find("EST-F-901") == 0);
        }
    }
    // the frozen sizing is the one the registration names (figures copied from exit_gate_fd_sizing_run1.log)
    {
        const auto& a = frozen_of(rows()[0]);
        CHECK(a.nu == 0.00012361537665128708);
        CHECK(a.h[0] == 25.055152903480732);
        CHECK(a.h[3] == 0.025055152903480732);
        CHECK(a.delta_fd == 1.7860642149272963e-05);
        const auto& b = frozen_of(rows()[1]);
        CHECK(b.nu == 0.00012161117047071457);
        CHECK(b.delta_fd == 1.7527314418192749e-05);
        const auto& c = frozen_of(rows()[2]);
        CHECK(c.nu == 9.2413276433944702e-05);
        CHECK(c.delta_fd == 1.2677626011087388e-05);
    }
    // (b) the fits, gated on the status set, N and the gross-error factor; (c) the reported variants
    for (const Row& r : rows()) {
        const Data d = read_row(r);
        const Theta th0 = nominal_start(d);
        Fit nominal_cb;
        for (bool box : {false, true}) {
            Config c;
            c.box_wing = box;
            const Model m(r, d, c);
            const Fit f = fit7(m, th0);
            print_fit(r, box ? "box-wing fit" : "cannonball fit", m, f);
            const auto st = f.lm.status;
            CHECK((st == est::LmStatus::converged_gradient || st == est::LmStatus::converged_step || st == est::LmStatus::stalled));
            CHECK(kRows / 3 == static_cast<std::size_t>(kEpochs));
            CHECK(f.sn_start / f.sn_end >= 10.0);
            if (!box) nominal_cb = f;
        }
        const double sn0 = nominal_cb.sn_end, c0 = nominal_cb.hat[6];
        auto variant = [&](const char* label, const Config& c, const Theta& start) {
            const Model m(r, d, c);
            const Fit f = fit7(m, start);
            print_fit(r, label, m, f);
            return f;
        };
        { Config c; c.degree = 12; (void)variant("REPORTED cannonball, gravity degree 12", c, th0); }
        { Config c; c.set_b = true; (void)variant("REPORTED cannonball, set B planets", c, th0); }
        { Config c; c.desai = true; (void)variant("REPORTED cannonball, Desai ocean pole tide", c, th0); }
        {
            const Config c;
            const Fit f = variant("REPORTED cannonball, perturbed start (+1 km, +1 m/s, srp_scale = 2)", c, perturbed(th0));
            std::printf("  prediction (not gated): perturbed-start S_N agrees with the nominal's to 1e-3 relative: |dS_N|/S_N = %.3g -> %s\n", std::fabs(f.sn_end - sn0) / sn0, std::fabs(f.sn_end - sn0) / sn0 <= 1e-3 ? "met" : "NOT met");
        }
        for (double fac : {2.0, 0.5}) {
            Config c;
            c.area_factor = fac;
            const Fit f = variant(fac == 2.0 ? "REPORTED cannonball, A C_R/m x2" : "REPORTED cannonball, A C_R/m x0.5", c, th0);
            std::printf("  prediction (not gated): S_N unchanged to 1e-6 relative: |dS_N|/S_N = %.3g -> %s; c scales inversely: c*factor/c_nominal = %.9g (1 expected)\n", std::fabs(f.sn_end - sn0) / sn0, std::fabs(f.sn_end - sn0) / sn0 <= 1e-6 ? "met" : "NOT met", f.hat[6] * fac / c0);
        }
    }
}

TEST_CASE("EST-A-901 the finite-difference correlations of the seven-parameter fit", "[est][exit][901]") {
    std::size_t ai = 0;
    for (const Row& r : rows()) {
        const auto& z = frozen_of(r);
        const Data d = read_row(r);
        const Theta th0 = nominal_start(d);
        const Model m(r, d, Config{});
        const Fit f = fit7(m, th0);
        print_fit(r, "cannonball fit (EST-A-901)", m, f);
        REQUIRE(f.lm.final_system.has_value());
        const auto& sol = *f.lm.final_system;
        auto layout = est::JointParameterLayout::create({"x", "y", "z", "vx", "vy", "vz"}, {"srp_scale"});
        auto jc = est::joint_covariance(sol, *layout);
        REQUIRE(jc.has_value());
        const double nu_shadow = shadow_min(m, f);
        const bool gated = nu_shadow >= 1.0;
        // J_FD by central differences of the residual (model calls only), the frozen steps
        std::vector<double> jfd(kRows * kNp, 0.0);
        std::array<double, kNp> cn{};
        for (std::size_t j = 0; j < kNp; ++j) {
            Theta up = f.hat, dn = f.hat;
            up[j] += z.h[j];
            dn[j] -= z.h[j];
            const auto ru = m.residual(up), rd = m.residual(dn);
            for (std::size_t k = 0; k < kRows; ++k) jfd[k * kNp + j] = -(ru[k] - rd[k]) / (2.0 * z.h[j]);     // dq/dtheta, q = p - r
            double s = 0;
            for (std::size_t k = 0; k < kRows; ++k) s += jfd[k * kNp + j] * jfd[k * kNp + j];
            cn[j] = std::sqrt(s);
        }
        double delta_fd = 0;
        for (std::size_t j = 0; j < kNp; ++j) delta_fd = std::max(delta_fd, z.eps[j] / cn[j]);
        const double kappa = sol.condition.condition_estimate;
        const double tol = std::max(1e-6, 8.0 * kappa * delta_fd);
        double state_eps = 0;
        for (std::size_t j = 0; j < 6; ++j) state_eps = std::max(state_eps, z.eps[j] / cn[j]);
        const double tol_state = std::max(1e-6, 8.0 * kappa * state_eps);      // Amendment 1: the state-sized tolerance
        std::printf("  Amendment 1: tol_ij_state = max(1e-6, 8 kappa max_{j<6} eps_j/|J_FD,j|) = %.9g\n", tol_state);
        const auto rho_fd = correlation_of(jfd, kRows, kNp);
        std::printf("  EST-A-901 %s: shadow minimum nu = %.17g -> %s; kappa_est = %.9g, delta_FD = %.9g, tol_ij = %.9g\n", r.name, nu_shadow, gated ? "gated" : "printed, not gated (EST-Q-013)", kappa, delta_fd, tol);
        double worst = 0;
        std::size_t wi = 0, wj = 0;
        for (std::size_t i = 0; i < kNp; ++i)
            for (std::size_t j = i + 1; j < kNp; ++j) {
                const double dd = std::fabs(rho_fd[i * kNp + j] - jc->correlation[i * kNp + j]);
                std::printf("    pair (%s, %s): rho = %.9g, rho_FD = %.9g, |diff| = %.3g\n", names()[i].c_str(), names()[j].c_str(), jc->correlation[i * kNp + j], rho_fd[i * kNp + j], dd);
                if (dd > worst) { worst = dd; wi = i; wj = j; }
                if (gated) { CHECK(dd <= tol); CHECK(dd <= tol_state); }
            }
        std::printf("  largest |rho_FD - rho| = %.3g at (%s, %s) against tol_ij = %.3g\n", worst, names()[wi].c_str(), names()[wj].c_str(), tol);
        // each column of the fit's final analytic Jacobian against J_FD,j (relative to the column's norm)
        for (std::size_t j = 0; j < kNp; ++j) {
            double mx = 0;
            for (std::size_t k = 0; k < kRows; ++k) mx = std::max(mx, std::fabs(f.at_hat.jac[k * kNp + j] - jfd[k * kNp + j]));
            const double rel = mx / cn[j], bound = 8.0 * z.eps[j] / cn[j];
            std::printf("    column %s: max|J - J_FD|/|J_FD,j| = %.3g against 8 eps_j/|J_FD,j| = %.3g\n", names()[j].c_str(), rel, bound);
            if (gated) CHECK(rel <= bound);
        }
        // control 1: the SRP column's sign flipped on the covariance side
        {
            auto flipped = f.at_hat.jac;
            for (std::size_t k = 0; k < kRows; ++k) flipped[k * kNp + 6] = -flipped[k * kNp + 6];
            const auto rho_f = correlation_of(flipped, kRows, kNp);
            double mx = 0;
            for (std::size_t j = 0; j < 6; ++j) mx = std::max(mx, std::fabs(rho_f[6 * kNp + j] - jc->correlation[6 * kNp + j]));
            std::printf("  control 1 (SRP column sign flipped): largest |delta rho| over the six pairs touching srp_scale = %.9g against 10 tol_ij = %.9g -> %s\n", mx, 10.0 * tol, mx >= 10.0 * tol ? "caught" : "NOT caught");
            double c1 = 0;
            for (std::size_t j = 0; j < 6; ++j) c1 = std::max(c1, std::fabs(jc->correlation[6 * kNp + j]));
            std::printf("  Amendment 1, control 1: >= 2 tol_ij = %.9g and >= 0.9 (2 max|rho_j,c|) = %.9g and >= 1e3 worst pair diff = %.9g\n", 2.0 * tol, 0.9 * 2.0 * c1, 1e3 * worst);
            CHECK(mx >= 2.0 * tol);
            CHECK(mx >= 0.9 * 2.0 * c1);
            CHECK(mx >= 1e3 * worst);
        }
        // control 2: the SRP parameter omitted (a six-parameter refit, srp_scale held at 1)
        {
            const Fit g = run_fit(m, th0, {0, 1, 2, 3, 4, 5}, {"x", "y", "z", "vx", "vy", "vz"});
            std::printf("  control 2: six-parameter refit: status %s, S_N = %.9g m\n", est::to_string(g.lm.status), g.sn_end);
            std::vector<double> j6(kRows * 6);
            for (std::size_t k = 0; k < kRows; ++k)
                for (std::size_t j = 0; j < 6; ++j) j6[k * 6 + j] = g.at_hat.jac[k * kNp + j];
            const auto rho6 = correlation_of(j6, kRows, 6);
            double mx = 0;
            for (std::size_t i = 0; i < 6; ++i)
                for (std::size_t j = i + 1; j < 6; ++j) mx = std::max(mx, std::fabs(rho6[i * 6 + j] - jc->correlation[i * kNp + j]));
            std::printf("  control 2 (SRP omitted): largest change of the other fifteen correlations = %.9g against 10 tol_ij = %.9g -> %s\n", mx, 10.0 * tol, mx >= 10.0 * tol ? "caught" : "NOT caught");
            double c2 = 0;
            for (std::size_t i = 0; i < 6; ++i)
                for (std::size_t j = i + 1; j < 6; ++j) c2 = std::max(c2, std::fabs(jc->correlation[i * kNp + 6] * jc->correlation[j * kNp + 6]));
            std::printf("  Amendment 1, control 2: >= 10 tol_state = %.9g and >= 0.9 max|rho_i,c rho_j,c| = %.9g and >= 1e3 worst pair diff = %.9g\n", 10.0 * tol_state, 0.9 * c2, 1e3 * worst);
            CHECK(mx >= 10.0 * tol_state);
            CHECK(mx >= 0.9 * c2);
            CHECK(mx >= 1e3 * worst);
        }
        ++ai;
    }
    (void)ai;
}

TEST_CASE("EST-A-902 the orbit fit's two-parameter regions against the objective's own profile", "[est][exit][902]") {
    for (const Row& r : rows()) {
        const auto& z = frozen_of(r);
        const Data d = read_row(r);
        const Theta th0 = nominal_start(d);
        const Model m(r, d, Config{});
        const Fit f = fit7(m, th0);
        print_fit(r, "cannonball fit (EST-A-902)", m, f);
        REQUIRE(f.lm.final_system.has_value());
        const auto& sol = *f.lm.final_system;
        auto layout = est::JointParameterLayout::create({"x", "y", "z", "vx", "vy", "vz"}, {"srp_scale"});
        auto jc = est::joint_covariance(sol, *layout);
        REQUIRE(jc.has_value());
        // the pairs, by the rules, from the start's covariance
        Evaluation e0;
        REQUIRE_FALSE(m.evaluate(th0, true, e0).has_value());
        const auto rho0 = correlation_of(e0.jac, kRows, kNp);
        std::size_t p1 = 0;
        for (std::size_t j = 1; j < 6; ++j)
            if (std::fabs(rho0[6 * kNp + j]) > std::fabs(rho0[6 * kNp + p1])) p1 = j;
        std::size_t a2 = 0, b2 = 1;
        for (std::size_t i = 0; i < 6; ++i)
            for (std::size_t j = i + 1; j < 6; ++j)
                if (std::fabs(rho0[i * kNp + j]) > std::fabs(rho0[a2 * kNp + b2])) { a2 = i; b2 = j; }
        const std::array<std::array<std::size_t, 2>, 2> pairs{{{p1, 6}, {a2, b2}}};
        std::printf("  EST-A-902 %s: pairs chosen by the rules from the START's covariance: P1 = (%s, srp_scale) with rho_start = %.9g; P2 = (%s, %s) with rho_start = %.9g\n", r.name, names()[p1].c_str(), rho0[6 * kNp + p1], names()[a2].c_str(), names()[b2].c_str(), rho0[a2 * kNp + b2]);
        const double omega_hat = sum_sq(f.at_hat.r);
        const auto s0sq = sol.sigma0_squared();
        REQUIRE(s0sq.has_value());
        std::printf("  Omega-hat = %.9g, sigma-hat_0^2 = %.9g, nu = %td\n", omega_hat, *s0sq, sol.degrees_of_freedom);
        for (std::size_t pi = 0; pi < 2; ++pi) {
            const std::size_t a = pairs[pi][0], b = pairs[pi][1];
            const std::string na = names()[a], nb = names()[b];
            auto ell = est::two_parameter_ellipse(sol, na, nb, est::ConfidenceMode::estimated_sigma0, 0.95);
            REQUIRE(ell.has_value());
            // H_r on the pair block, by second differences at the frozen steps h2
            const double ha = z.h2[a], hb = z.h2[b];
            auto at = [&](double sa, double sb) { Theta t = f.hat; t[a] += sa * ha; t[b] += sb * hb; return m.residual(t); };
            const auto& r0 = f.at_hat.r;
            const auto pa = at(1, 0), ma = at(-1, 0), pb = at(0, 1), mb = at(0, -1), pp = at(1, 1), pm = at(1, -1), mp = at(-1, 1), mm = at(-1, -1);
            double haa = 0, hbb = 0, hab = 0;
            for (std::size_t k = 0; k < kRows; ++k) {
                haa += r0[k] * (pa[k] - 2 * r0[k] + ma[k]) / (ha * ha);
                hbb += r0[k] * (pb[k] - 2 * r0[k] + mb[k]) / (hb * hb);
                hab += r0[k] * (pp[k] - pm[k] - mp[k] + mm[k]) / (4 * ha * hb);
            }
            // scaled variables of EST-R-201: theta = S y, S = diag(2^e): N_s and H_s are the pair's blocks of S N S and S H S
            const double sa = std::ldexp(1.0, -sol.scale_exponents[a]), sb = std::ldexp(1.0, -sol.scale_exponents[b]);
            const double n11 = sol.scaled_matrix[a * kNp + a], n12 = sol.scaled_matrix[a * kNp + b], n22 = sol.scaled_matrix[b * kNp + b];
            const double h11 = sa * sa * haa, h12 = sa * sb * hab, h22 = sb * sb * hbb;
            // eigenvalues of N^-1 H (similar to N^-1/2 H N^-1/2): closed form for a 2x2
            const double det = n11 * n22 - n12 * n12;
            const double m11 = (n22 * h11 - n12 * h12) / det, m12 = (n22 * h12 - n12 * h22) / det, m21 = (-n12 * h11 + n11 * h12) / det, m22 = (-n12 * h12 + n11 * h22) / det;
            const double tr = m11 + m22, dt = m11 * m22 - m12 * m21, disc = std::sqrt(std::max(0.0, tr * tr / 4 - dt));
            const double lam = std::max(std::fabs(tr / 2 + disc), std::fabs(tr / 2 - disc));
            const double bpair = 2.0 * lam;
            const double rho_ab = jc->correlation[a * kNp + b];
            std::printf("  pair %s: (%s, %s): rho = %.9g; ellipse p = 0.95 estimated_sigma0: level 2F = %.9g, semi-axes %.9g %.9g, angle %.9g; H_r block (haa, hab, hbb) = (%.9g, %.9g, %.9g), h2 = (%.9g, %.9g); |lambda|_max = %.9g; b_pair (per unit s) = %.9g (predicted b_pair <= 0.1: %s)\n",
                        pi == 0 ? "P1" : "P2", na.c_str(), nb.c_str(), rho_ab, ell->level, ell->semi_major, ell->semi_minor, ell->angle, haa, hab, hbb, ha, hb, lam, bpair, bpair <= 0.1 ? "met" : "NOT met");
            { const double pl05 = *s0sq * ell->level * 0.25, pl10 = *s0sq * ell->level; const double bn05 = 2.0 * std::sqrt(omega_hat) * z.nu / pl05, bn10 = 2.0 * std::sqrt(omega_hat) * z.nu / pl10;
              std::printf("  Amendment 1: b_curv (per unit s) = %.9g; b_noise(0.5) = %.9g, b_noise(1.0) = %.9g (2 sqrt(Omega-hat) nu / P_lin, nu = %.9g); prediction b_curv + b_noise(1.0) <= 0.1: %s (%.9g)\n", bpair, bn05, bn10, z.nu, bpair + bn10 <= 0.1 ? "met" : "NOT met", bpair + bn10); }
            std::fflush(stdout);
            // the profile, at s in {0.5, 1} on eight points
            std::array<double, 3> cov = ell->covariance;
            auto quad = [&](const std::array<double, 3>& c, double dx, double dy) { const double dd = c[0] * c[2] - c[1] * c[1]; return (c[2] * dx * dx - 2 * c[1] * dx * dy + c[0] * dy * dy) / dd; };
            std::vector<std::size_t> others;
            for (std::size_t j = 0; j < kNp; ++j) if (j != a && j != b) others.push_back(j);
            std::vector<std::string> onames;
            for (auto j : others) onames.push_back(names()[j]);
            double worst_flipped = 0;
            for (double s : {0.5, 1.0})
                for (int kk = 0; kk < 8; ++kk) {
                    const auto bp = ell->boundary_point(2.0 * std::numbers::pi * kk / 8.0);
                    const double dx = s * (bp[0] - ell->centre[0]), dy = s * (bp[1] - ell->centre[1]);
                    Theta base = f.hat;
                    base[a] += dx;
                    base[b] += dy;
                    std::vector<double> x0;
                    for (auto j : others) x0.push_back(f.hat[j]);
                    est::LmOptions opt;
                    opt.max_iterations = 100;
                    const auto res = est::levenberg_marquardt(onames, x0, lm_model(m, base, others), opt);
                    const double P = 2.0 * res.objective - omega_hat;
                    const double plin = *s0sq * quad(cov, dx, dy);
                    const double rel = P / plin - 1.0;
                    std::printf("    %s s = %.1f t_%d: LM status %s (%zu passes); P = %.9g, P_lin = %.9g, P/P_lin - 1 = %.6g against b_pair s = %.6g%s\n", pi == 0 ? "P1" : "P2", s, kk, est::to_string(res.status), res.iterations, P, plin, rel, bpair * s, s == 0.5 ? (P <= *s0sq * ell->level ? "; inside the region by the profile" : "; NOT inside the region by the profile") : "");
                    std::fflush(stdout);
                    const double bnoise = 2.0 * std::sqrt(omega_hat) * z.nu / plin;
                    std::printf("      amended gate (Amendment 1): b_curv s + b_noise = %.6g + %.6g = %.6g\n", bpair * s, bnoise, bpair * s + bnoise);
                    CHECK(std::fabs(rel) <= bpair * s + bnoise);
                    if (s == 0.5) CHECK(P <= *s0sq * ell->level);
                    if (s == 1.0) {
                        std::array<double, 3> flip{cov[0], -cov[1], cov[2]};
                        const double plf = *s0sq * quad(flip, dx, dy);
                        worst_flipped = std::max(worst_flipped, std::fabs(P / plf - 1.0));
                    }
                }
            if (std::fabs(rho_ab) >= 0.5) {
                std::printf("  control (sign-flipped correlation) %s: largest |P/P_lin,flipped - 1| at s = 1 = %.9g against 0.1 -> %s\n", pi == 0 ? "P1" : "P2", worst_flipped, worst_flipped >= 0.1 ? "caught" : "NOT caught");
                CHECK(worst_flipped >= 0.1);
            } else std::printf("  control (sign-flipped correlation) %s: |rho| = %.9g < 0.5: not applicable (registered: for a pair with |rho| >= 0.5); the largest |P/P_lin,flipped - 1| at s = 1 is %.9g\n", pi == 0 ? "P1" : "P2", std::fabs(rho_ab), worst_flipped);
        }
    }
}
