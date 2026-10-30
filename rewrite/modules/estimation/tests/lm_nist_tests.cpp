// lm_nist_tests.cpp — SPEC-estimation.md §8.3: Levenberg–Marquardt on NIST's 27 nonlinear datasets, from start 1, start 2 and the certified point (81 runs), with the two controls
// (plain Gauss–Newton; DTU's Algorithm 3.16 unscaled).  The certified numbers are NIST's (Statistical Reference Datasets, SRD 140), read from the pinned files.
//
// WHAT IS GATED (registered in C12_..., L7_step3_registration.txt, before the first run of this file; the manager's ruling of 2026-10-31):
//   G1  from the CERTIFIED point: the least LRE over the parameters is >= kLreCertified (4) for ALL 27 sets (the LRE is not widened for the near-zero-residual ones); any status other than
//       converged_* from the certified point is LISTED BY NAME with F and the gradient cosine (lines "LMNOTE"), not gated: at the rounded certified point the cosine of Lanczos1 and its like
//       may be rounding noise;
//   G2  THE STATUS NEVER LIES: any run, from any start, that reports converged_* EITHER matches the certified values to LRE >= kLreCertified OR stands at a different stationary point
//       with a LARGER objective, shown by an INDEPENDENT gradient (central differences of the double model, not the code's own Jacobian: cosine <= kFdCosine);
//   G3  a SMALLER objective than NIST's, from any run, is a FINDING (the test fails and says which): smaller by more than the printed precision of the certified sum (kFPrinted = 1e-10 relative)
//       plus the absolute rounding floor of noise_floor();
//   G4  THE CONTROL HAS POWER: plain Gauss–Newton fails (does not reach F <= F_NIST (1 + 1e-6)) from at least one of the 54 starts.
// Every other status is RECORDED, not gated; the counts are printed (lines starting "LMRUN" and "LMCOUNT") and reported unpredicted.
#include "lm_controls.hpp"
#include "nist_nls.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <map>

using namespace odl::estimation;
using namespace odl::estimation::fx;

namespace {

constexpr double kLreCertified = 4.0;
constexpr double kFdCosine = 1.0e-6;
constexpr double kFRelative = 1.0e-8;           // G2: "a larger objective" means larger than NIST's by more than this relative margin plus the floor
constexpr double kFPrinted = 1.0e-10;           // G3: "smaller" means smaller than NIST's by more than the PRINTED PRECISION of the certified sum (11 figures: a unit of the last place is at most 1e-10 relative) plus the floor

std::vector<std::string> param_names(std::size_t n) {
    std::vector<std::string> v;
    for (std::size_t i = 0; i < n; ++i) v.push_back("b" + std::to_string(i + 1));
    return v;
}

// the cosine of the residual with the central-difference columns of the DOUBLE model: independent of the Jacobian the fit used
double fd_cosine(const NlsData& d, const std::vector<double>& b) {
    std::vector<double> r(d.m);
    double rr = 0.0;
    for (std::size_t k = 0; k < d.m; ++k) {
        r[k] = d.y[k] - nls_model<double>(d.name, b.data(), &d.x[k * d.columns]);
        rr += r[k] * r[k];
    }
    if (!(rr > 0.0) || !std::isfinite(rr)) return std::numeric_limits<double>::quiet_NaN();
    double worst = 0.0;
    for (std::size_t i = 0; i < d.n; ++i) {
        const double h = 6.0e-6 * std::max(std::fabs(b[i]), 1.0e-8);
        std::vector<double> bp = b, bm = b;
        bp[i] += h;
        bm[i] -= h;
        double ar = 0.0, aa = 0.0;
        for (std::size_t k = 0; k < d.m; ++k) {
            const double col = (nls_model<double>(d.name, bp.data(), &d.x[k * d.columns]) - nls_model<double>(d.name, bm.data(), &d.x[k * d.columns])) / (2.0 * h);
            ar += col * r[k];
            aa += col * col;
        }
        if (aa > 0.0) worst = std::max(worst, std::fabs(ar) / (std::sqrt(aa) * std::sqrt(rr)));
    }
    return worst;
}

// the floor, ABSOLUTE, below which two objectives are the same number: the rounding of the residuals themselves, 2 u ymax sqrt(m) sqrt(2 F_nist) (the cross term) + 4 m u^2 ymax^2 (u = 2^-53).  It dominates
// only for the near-zero-residual sets (Lanczos1: F_nist = 7e-26) -- for every other set it is far below the relative margins.
double noise_floor(const NlsData& d, double f_nist) {
    double ymax = 0.0;
    for (double v : d.y) ymax = std::max(ymax, std::fabs(v));
    const double u = 1.1102230246251565e-16, md = static_cast<double>(d.m);
    return 2.0 * u * ymax * std::sqrt(md) * std::sqrt(2.0 * f_nist) + 4.0 * md * u * u * ymax * ymax;
}

bool converged(LmStatus s) { return s == LmStatus::converged_gradient || s == LmStatus::converged_residual || s == LmStatus::converged_step; }

struct Run {
    std::string name;
    int start = 0;                                   // 1, 2, 0 = certified
    LmResult lm;
    double lre = 0.0;
    double f_nist = 0.0;
    double fd_cos = 0.0;
    double floor = 0.0;
};

}  // namespace

TEST_CASE("EST-A-301: Levenberg–Marquardt on the 27 NIST nonlinear datasets: the certified point, the status that never lies, the finding rule, the control's power", "[estimation][lm][nist]") {
    std::vector<Run> runs;
    std::size_t gn_fail = 0, gn_total = 0, dtu_fail = 0;
    std::map<std::string, std::string> gn_note;
    for (const std::string& nm : nls_names()) {
        const NlsData d = load_nls(nm);
        REQUIRE(d.n >= 2);
        REQUIRE(d.m > d.n);
        const double f_nist = 0.5 * static_cast<double>(d.certified_rss);
        const std::vector<std::string> names = param_names(d.n);
        const LmModel model = nls_problem(d);
        std::vector<double> cert(d.n);
        for (std::size_t i = 0; i < d.n; ++i) cert[i] = static_cast<double>(d.certified[i]);
        for (int s : {1, 2, 0}) {
            const std::vector<double>& x0 = s == 1 ? d.start1 : s == 2 ? d.start2 : cert;
            Run r;
            r.name = nm;
            r.start = s;
            r.f_nist = f_nist;
            r.floor = noise_floor(d, f_nist);
            r.lm = levenberg_marquardt(names, x0, model);
            r.lre = lre_min(r.lm.x, d.certified);
            r.fd_cos = fd_cosine(d, r.lm.x);
            std::printf("LMRUN %-9s start=%d status=%-18s iters=%4zu acc=%4zu rej=%4zu F=%.12e F_nist=%.12e LRE=%5.2f fd_cos=%.2e mu=%.2e\n", nm.c_str(), s, to_string(r.lm.status),
                        r.lm.iterations, r.lm.accepted_steps, r.lm.rejected_steps, r.lm.objective, f_nist, r.lre, r.fd_cos, r.lm.mu);
            runs.push_back(r);
            if (s != 0) {
                ++gn_total;
                const ControlRun gn = gauss_newton(names, x0, model, 200);
                const bool ok = std::isfinite(gn.objective) && gn.objective <= f_nist * (1.0 + 1.0e-6) + noise_floor(d, f_nist);
                if (!ok) { ++gn_fail; }
                const ControlRun dt = dtu_unscaled(names, x0, model, 1.0e-3, 0.0, 1.0e-12, 1000);
                const bool dok = std::isfinite(dt.objective) && dt.objective <= f_nist * (1.0 + 1.0e-6) + noise_floor(d, f_nist);
                if (!dok) ++dtu_fail;
                std::printf("LMCTRL %-9s start=%d GN=%s(%s,%zu) DTUunscaled=%s(%s,k=%zu,F=%.6e)\n", nm.c_str(), s, ok ? "reaches" : "FAILS", gn.ended.c_str(), gn.iterations,
                            dok ? "reaches" : "FAILS", dt.ended.c_str(), dt.passes, dt.objective);
            }
        }
    }
    // the counts
    std::size_t conv[3] = {0, 0, 0}, reach[3] = {0, 0, 0}, total[3] = {0, 0, 0};
    std::map<LmStatus, std::size_t> by_status;
    for (const Run& r : runs) {
        const int slot = r.start == 0 ? 2 : r.start - 1;
        ++total[slot];
        if (converged(r.lm.status)) ++conv[slot];
        if (r.lm.objective <= r.f_nist * (1.0 + 1.0e-6) + r.floor) ++reach[slot];
        ++by_status[r.lm.status];
    }
    std::printf("LMCOUNT runs=%zu | converged_*: start1 %zu/%zu start2 %zu/%zu certified %zu/%zu | reach NIST's F: start1 %zu start2 %zu certified %zu\n", runs.size(), conv[0], total[0], conv[1],
                total[1], conv[2], total[2], reach[0], reach[1], reach[2]);
    for (const auto& [st, c] : by_status) std::printf("LMCOUNT status %-18s %zu\n", to_string(st), c);
    std::printf("LMCOUNT Gauss-Newton fails from %zu of %zu starts; DTU unscaled fails from %zu of %zu\n", gn_fail, gn_total, dtu_fail, gn_total);

    std::vector<std::string> other_stationary;
    for (const Run& r : runs) {
        INFO(r.name << " start " << r.start << " status " << to_string(r.lm.status) << " F=" << r.lm.objective << " F_nist=" << r.f_nist << " LRE=" << r.lre);
        // G3: a smaller objective than NIST's is a finding
        CHECK(r.lm.objective >= r.f_nist * (1.0 - kFPrinted) - r.floor);
        if (r.start == 0) {
            // G1
            if (!converged(r.lm.status))
                std::printf("LMNOTE certified-start %-9s status=%s F=%.12e F_nist=%.12e cosine=%.3e fd_cos=%.3e\n", r.name.c_str(), to_string(r.lm.status), r.lm.objective, r.f_nist, r.lm.cosine, r.fd_cos);
            CHECK(r.lre >= kLreCertified);
        }
        if (converged(r.lm.status)) {
            // G2
            if (r.lre < kLreCertified) {
                const bool larger = r.lm.objective > r.f_nist * (1.0 + kFRelative) + r.floor;
                const bool stationary = r.fd_cos <= kFdCosine;
                CHECK(larger);
                CHECK(stationary);
                other_stationary.push_back(r.name + "/start" + std::to_string(r.start));
            }
        }
    }
    std::string list;
    for (const auto& s : other_stationary) list += s + " ";
    std::printf("LMCOUNT converged runs at a DIFFERENT stationary point (%zu): %s\n", other_stationary.size(), list.c_str());
    // G4
    CHECK(gn_fail >= 1);
}
