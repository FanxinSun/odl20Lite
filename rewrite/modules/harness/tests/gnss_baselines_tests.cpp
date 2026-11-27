// gnss_baselines_tests.cpp — L8 step 1 (plan/subplan_L8/L8-1.md, registered 2026-11-27 and ruled L8-R1, before this file existed): the live gated baselines.
//   G-01, G-02, G-03 cannonball (EST-Q-011's block-referenced template): S_N within 1e-6 relative of the L7 exit gate's recorded minima 0.0622784085, 0.0696950657, 0.242196639 m, and S_N <= 2F (F = 0.0645, 0.0573, 0.1870 m).
//   G-01 box-wing (gps_block_iif()): S_N within 1e-6 relative of 0.0947894259 m.
//   G-01 ECOM2 (d4b1_order()): converged, S_N(start)/S_N(end) >= 10, S_N below the G-01 cannonball's recorded minimum, and S_N <= 2 x 0.0442 = 0.0884 m.
#include "harness_fixture.hpp"

#include <cmath>
#include <cstdio>

using namespace odl::harness;

namespace {

const std::string& sp3_text(int day) {
    static const std::string t22 = hfx::slurp(ODL_H_SP3_022), t23 = hfx::slurp(ODL_H_SP3_023);
    return day == 22 ? t22 : t23;
}

void print_fit(const char* label, const GnssFitResult& f) {
    std::printf("  %s: status %s (%s); passes %zu (accepted %zu, rejected %zu); S_N(start) = %.10g m, S_N = %.10g m (ratio %.6g), S_{N-p} = %.10g m, S_N/sqrt(3) = %.10g m; RMS R/T/N %.9g / %.9g / %.9g m; max epoch %.9g m; min nu %.17g\n",
                label, f.status.c_str(), f.message.c_str(), f.passes, f.accepted, f.rejected, f.s_n_start_m, f.stats.s_n_m, f.start_over_end(), f.stats.s_n_minus_p_m, f.stats.s_n_over_sqrt3_m, f.stats.rms_radial_m, f.stats.rms_along_m, f.stats.rms_cross_m, f.stats.max_epoch_m, f.min_shadow_nu);
    std::printf("    parameters:");
    for (std::size_t i = 0; i < f.parameter_names.size(); ++i) std::printf(" %s=%.12g", f.parameter_names[i].c_str(), f.theta_hat[i]);
    std::printf("\n");
    std::fflush(stdout);
}

struct Row { const char* name; int day; const char* sat; bool iif; double recorded_sn; double F; };

void run_cannonball_row(const Row& r) {
    auto arc = read_sp3_arc(hfx::world(), sp3_text(r.day), r.sat);
    REQUIRE(arc.has_value());
    auto tpl = block_referenced_template(r.iif);
    REQUIRE(tpl.has_value());
    std::printf("  %s template: A = %.17g m^2, m = %.17g kg, C_R = %.17g\n", r.name, tpl->area_m2, tpl->mass_kg, tpl->cr);
    auto fit = fit_gnss_cannonball(hfx::world(), *arc, *tpl);
    REQUIRE(fit.has_value());
    print_fit(r.name, *fit);
    CHECK(fit->converged);
    CHECK(std::fabs(fit->stats.s_n_m - r.recorded_sn) <= 1e-6 * r.recorded_sn);
    CHECK(fit->stats.s_n_m <= 2.0 * r.F);
    CHECK(fit->start_over_end() >= 10.0);
}

}  // namespace

TEST_CASE("HARN-A-010 G-01 cannonball reproduces the L7 minimum", "[harness][gnss][g01]") { run_cannonball_row({"G-01", 22, "G01", true, 0.0622784085, 0.0645}); }
TEST_CASE("HARN-A-011 G-02 cannonball reproduces the L7 minimum", "[harness][gnss][g02]") { run_cannonball_row({"G-02", 23, "G01", true, 0.0696950657, 0.0573}); }
TEST_CASE("HARN-A-012 G-03 cannonball reproduces the L7 minimum", "[harness][gnss][g03]") { run_cannonball_row({"G-03", 22, "G05", false, 0.242196639, 0.1870}); }

TEST_CASE("HARN-A-013 G-01 box-wing reproduces the L7 minimum", "[harness][gnss][boxwing]") {
    auto arc = read_sp3_arc(hfx::world(), sp3_text(22), "G01");
    REQUIRE(arc.has_value());
    auto fit = fit_gnss_box_wing(hfx::world(), *arc, true);
    REQUIRE(fit.has_value());
    print_fit("G-01 box-wing", *fit);
    CHECK(std::fabs(fit->stats.s_n_m - 0.0947894259) <= 1e-6 * 0.0947894259);
}

TEST_CASE("HARN-A-014 G-01 ECOM2 d4b1 converges below the cannonball and inside the gross-error bound", "[harness][gnss][ecom]") {
    auto arc = read_sp3_arc(hfx::world(), sp3_text(22), "G01");
    REQUIRE(arc.has_value());
    auto fit = fit_gnss_ecom2(hfx::world(), *arc);
    REQUIRE(fit.has_value());
    print_fit("G-01 ECOM2 d4b1", *fit);
    CHECK(fit->converged);
    CHECK(fit->start_over_end() >= 10.0);
    CHECK(fit->stats.s_n_m < 0.0622784085);          // the G-01 cannonball's recorded minimum
    CHECK(fit->stats.s_n_m <= 2.0 * 0.0442);          // E-02's gross-error bound
}
