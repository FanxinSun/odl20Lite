// arc_geometry_tests.cpp — L7 step 3, part 2: the GEOMETRY and the FROZEN DATA of the two arcs, looked at before any estimator touches them.  Nothing here fits anything.
// Hidden by default ([.geometry]); it prints the numbers part 2 of the report states.
#include "arc_model.hpp"

#include <chrono>
#include <cstdio>

using namespace l7arc;

namespace {

double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

constexpr double kGridStep = 60.0;

ArcDesign gate_design(double raan_deg) {
    return ArcDesign{"gate-noeclipse", 2025, 12, 20, 0, 6378137.0 + 720e3, 0.001, 98.27, raan_deg, 90.0, 0.0, 3.0, 60.0};
}
ArcDesign eclipsing_design() {
    return ArcDesign{"eclipsing", 2025, 12, 20, 0, 6378137.0 + 720e3, 0.001, 24.0, 100.0, 0.0, 0.0, 3.0, 60.0};
}

}  // namespace

TEST_CASE("ARC-X-000  the two arcs' geometry, schedule and frozen data", "[.geometry]") {
    ArcForces forces;
    double raan = 0.0;
    {
        const ArcDesign probe = gate_design(0.0);
        const Vec3 s = sun_m(epoch_of(probe));
        const double ra = std::atan2(s.y, s.x) * 180.0 / kPi;
        std::printf("ARCGEOM sun_ra_deg=%.4f sun_dec_deg=%.4f\n", ra, std::asin(s.z / s.norm()) * 180.0 / kPi);
        raan = std::round((ra + 90.0) * 2.0) / 2.0;       // the Sun's right ascension + 90 deg, to half a degree (frozen in the report)
    }
    for (const ArcDesign& d : {gate_design(raan), eclipsing_design()}) {
        const odl::time::Epoch t0 = epoch_of(d);
        const auto truth = cartesian_of(d);
        const auto x0 = state_from(t0, truth);
        const double w0 = now_s();
        const auto grid = state_grid(forces, x0, d.days * 86400.0, kGridStep);
        const double w1 = now_s();
        const auto ill = illumination(grid, kGridStep, 10.0);
        const double w2 = now_s();
        std::printf("ARCGEOM arc=%s raan=%.3f inc=%.3f grid_states=%zu grid_s=%.1f illum_s=%.1f\n", d.name, d.raan_deg, d.inc_deg, grid.size(), w1 - w0, w2 - w1);
        std::printf("ARCGEOM arc=%s min_clearance_deg=%.4f min_nu=%.6f dark_s=%.0f samples=%zu step_s=%.0f edge_crossings=%zu\n", d.name, ill.min_clearance_rad * 180.0 / kPi,
                    ill.min_nu, ill.dark_seconds, ill.samples, ill.sample_step_s, ill.edge_crossings);
        ArcModel arc(forces, d, grid, kGridStep);
        std::printf("ARCGEOM arc=%s observations=%zu epochs=%zu", d.name, arc.count(), arc.epochs().size());
        for (std::size_t s = 0; s < kStations.size(); ++s) std::printf(" %s=%zu", kStations[s].name, arc.count_of_station(s));
        std::printf("\n");
        const double w3 = now_s();
        arc.generate_data(truth, 0, 0.0);
        const double w4 = now_s();
        auto at_truth = arc.evaluate(truth, true);
        const double w5 = now_s();
        auto at_truth_plain = arc.evaluate(truth, false);
        const double w6 = now_s();
        REQUIRE(at_truth.has_value());
        REQUIRE(at_truth_plain.has_value());
        double worst = 0.0;
        for (double r : at_truth->residual_m) worst = std::max(worst, std::abs(r));
        std::printf("ARCGEOM arc=%s data_gen_s=%.1f eval_with_stm_s=%.2f eval_plain_s=%.2f residual_at_truth_max_m=%.3e\n", d.name, w4 - w3, w5 - w4, w6 - w5, worst);
        std::printf("ARCGEOM arc=%s schedule_hash=%s data_hash=%s\n", d.name, arc.schedule_hash().c_str(), arc.data_hash().c_str());
        // the Jacobian against central differences of the model (fixture check): steps 100 m and 0.1 m/s
        const double steps[6] = {100.0, 100.0, 100.0, 0.1, 0.1, 0.1};
        for (std::size_t j = 0; j < 6; ++j) {
            auto xp = truth, xm = truth;
            xp[j] += steps[j]; xm[j] -= steps[j];
            auto ep = arc.evaluate(xp, false), em = arc.evaluate(xm, false);
            REQUIRE(ep.has_value());
            REQUIRE(em.has_value());
            double colmax = 0.0, diffmax = 0.0;
            for (std::size_t k = 0; k < arc.count(); ++k) {
                const double fd = -(ep->residual_m[k] - em->residual_m[k]) / (2.0 * steps[j]);
                const double an = at_truth->rows[k * 6 + j];
                colmax = std::max(colmax, std::abs(an));
                diffmax = std::max(diffmax, std::abs(fd - an));
            }
            std::printf("ARCGEOM arc=%s jacobian_column=%zu max_abs=%.4e max_difference_vs_central_difference=%.3e relative=%.3e\n", d.name, j, colmax, diffmax, diffmax / colmax);
        }
        // the ladder: the illumination of every rung's starting trajectory (gate arc only: the eclipsing arc is recorded, not gated)
        if (std::string(d.name) == "gate-noeclipse") {
            for (double s : kLadder) {
                const auto start = rung_start(truth, s);
                const auto g = state_grid(forces, state_from(t0, start), d.days * 86400.0, kGridStep);
                const auto il = illumination(g, kGridStep, 10.0);
                std::printf("ARCGEOM ladder s=%.0f min_clearance_deg=%.4f min_nu=%.6f dark_s=%.0f\n", s, il.min_clearance_rad * 180.0 / kPi, il.min_nu, il.dark_seconds);
            }
        }
    }
}
