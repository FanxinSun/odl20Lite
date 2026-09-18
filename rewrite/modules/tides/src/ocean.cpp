#include <odl/tides/ocean.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace odl::tides {

namespace {
constexpr double kGm = 3.986004415e14;      ///< EGM2008's, because the identity is the field's
constexpr double kAe = 6378136.3;           ///< EGM2008's reference radius, likewise
}  // namespace

std::array<double, 6> doodson_arguments(const eop::tides::Arguments& a) noexcept {
    // tau = gamma - s, s = F + Omega, h = s - D, p = s - l, N' = -Omega,
    // ps = s - D - l'.  Verified against TN36-6 Table 6.5a, which prints both
    // the Doodson and the Delaunay multipliers for every constituent.
    const double s = a.F + a.Om;
    return {a.gamma - s, s, s - a.D, s - a.l, -a.Om, s - a.D - a.lp};
}

odl::Result<OceanTide, TidesError>
OceanTide::load(const std::string& path, const std::string& cache_root) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
    const fs::path root = fs::weakly_canonical(fs::path(cache_root), ec);
    const auto rel = canonical.lexically_relative(root);
    if (root.empty() || rel.empty() || *rel.begin() == "..") {
        return odl::err(TidesError{"PERT-F-010",
                                   "the ocean tide file is outside the manifest cache: "
                                   + canonical.string()});
    }
    std::ifstream f(canonical);
    if (!f) return odl::err(TidesError{"PERT-F-010", "cannot open " + canonical.string()});

    OceanTide out;
    out.path_ = canonical.string();
    std::string l1, l2, l3, l4;
    std::getline(f, l1);
    std::getline(f, l2);
    std::getline(f, l3);
    std::getline(f, l4);

    // READ THE HEADER, DO NOT ASSUME IT.
    auto has = [](const std::string& s, const char* what) {
        return s.find(what) != std::string::npos;
    };
    out.header_.model = l2;
    out.header_.unit = has(l1, "10^-11") ? 1e-11 : 0.0;
    out.header_.max_degree = has(l2, "(100,100)") ? 100 : 0;
    out.header_.max_order = out.header_.max_degree;
    out.header_.long_period_from_fes2002 = has(l3, "FES2002");
    out.header_.long_period_max_degree = has(l3, "(50,50)") ? 50 : 0;
    out.header_.includes_equilibrium_omega = has(l3, "equilibrium Om1/Om2");
    out.header_.includes_atmospheric_tide = !has(l3, "atmospheric tide NOT included");

    if (out.header_.unit == 0.0 || out.header_.max_degree != 100
        || !out.header_.long_period_from_fes2002 || out.header_.long_period_max_degree != 50
        || !out.header_.includes_equilibrium_omega || out.header_.includes_atmospheric_tide) {
        std::ostringstream m;
        m << "the ocean tide file's header is not what this specification was written against.\n"
             "  expected  unit 10^-11; (100,100); long period from FES2002 up to (50,50); "
             "equilibrium Om1/Om2 INCLUDED; atmospheric tide NOT included\n"
             "  found     \"" << l1 << "\"\n            \"" << l2 << "\"\n            \""
          << l3 << "\"\n"
             "  This matters beyond bookkeeping: TN36-6 §6.3.2 tells an implementer to add the "
             "equilibrium Om1 and Om2 waves, and this file already contains them. A file whose "
             "header differs may not, and then the chapter's instruction becomes right again.";
        return odl::err(TidesError{"PERT-F-011", m.str()});
    }

    std::string line;
    std::string previous_doodson;
    while (std::getline(f, line)) {
        std::istringstream s(line);
        std::string doodson, darwin;
        Row r{};
        if (!(s >> doodson >> darwin >> r.n >> r.m >> r.cp >> r.sp >> r.cm >> r.sm)) continue;
        // Doodson number nnn.nnn: the six multipliers are the digits, with 5
        // subtracted from all but the first.
        // THE LONG-PERIOD WAVES CARRY FIVE DIGITS, NOT SIX.  Their first
        // Doodson multiplier is zero and the file prints "55.565" where a
        // diurnal wave is "165.555", so a parser that demands six digits drops
        // every long-period wave — 7952 of 59462 rows, and 8 of the 18 waves,
        // silently. The row count is what caught it.
        std::string digits;
        for (char c : doodson) {
            if (c >= '0' && c <= '9') digits.push_back(c);
        }
        while (digits.size() < 6) digits.insert(digits.begin(), '0');
        if (digits.size() != 6) continue;
        r.doodson[0] = digits[0] - '0';
        for (int i = 1; i < 6; ++i) r.doodson[i] = (digits[static_cast<std::size_t>(i)] - '0') - 5;
        if (doodson != previous_doodson) {
            ++out.wave_count_;
            previous_doodson = doodson;
        }
        out.rows_.push_back(r);
    }
    if (out.rows_.size() < 10000) {
        std::ostringstream m;
        m << "only " << out.rows_.size() << " coefficient rows were read from "
          << canonical.string() << "; the file carries tens of thousands.";
        return odl::err(TidesError{"PERT-F-011", m.str()});
    }
    out.variance_by_degree_.assign(101, 0.0);
    for (const Row& r : out.rows_) {
        if (r.n > 100) continue;
        const double u = out.header_.unit;
        out.variance_by_degree_[static_cast<std::size_t>(r.n)] +=
            u * u * (r.cp * r.cp + r.sp * r.sp + r.cm * r.cm + r.sm * r.sm);
    }
    return out;
}

double OceanTide::truncation_rms(double radius_m, int degree) const noexcept {
    // The same identity the static field's truncation uses (GRAV-R-041), so the
    // two numbers are comparable:
    //   rms|a_n| = (GM/r^2)(ae/r)^n sigma_n sqrt((n+1)(2n+1))
    double total = 0.0;
    double power = 1.0;
    for (int n = 0; n <= 100; ++n) {
        if (n > degree) {
            const double s = std::sqrt(variance_by_degree_[static_cast<std::size_t>(n)]) * power;
            total += s * s * (n + 1.0) * (2.0 * n + 1.0);
        }
        power *= kAe / radius_m;
    }
    return kGm / (radius_m * radius_m) * std::sqrt(total);
}

double OceanTide::smallest_kept_rms(double radius_m, int) const noexcept {
    // THE SMALLEST TERM THIS MODULE COMPUTES AND KEEPS, and it is not a function
    // of the ocean tide's own truncation.  The first version read it as the
    // smallest per-degree RMS among the degrees KEPT, which is a moving target:
    // keeping more degrees lowers the bar, so the criterion chased itself down
    // to degree 99 and said "keep everything".
    //
    // The fixed threshold is TN36-6 §6.2.1's OWN CUTOFF for what the solid Earth
    // tide includes: changes "exceeding 3e-12" in C4m and S4m, which the module
    // keeps and which are the smallest thing it deliberately keeps. At radius r
    // that is, by GRAV-R-041's identity,
    //     (GM/r^2) (ae/r)^4 * 3e-12 * sqrt(5*9)
    // — 8.552e-11 m/s^2 at 7331 km, which is SPEC-perturbations PERT-P-2.
    const double ratio = kAe / radius_m;
    const double r4 = ratio * ratio * ratio * ratio;
    return kGm / (radius_m * radius_m) * r4 * 3e-12 * std::sqrt(5.0 * 9.0);
}

int OceanTide::degree_meeting_criterion(double radius_m) const noexcept {
    const double bar = smallest_kept_rms(radius_m, 0);
    for (int d = 2; d <= 100; ++d) {
        if (truncation_rms(radius_m, d) < bar) return d;
    }
    return 100;
}

odl::Result<TideIncrements, TidesError>
OceanTide::increments(const eop::tides::Arguments& args, int max_degree, int max_order) const {
    if (max_degree < 2 || max_degree > header_.max_degree || max_order > max_degree
        || max_order < 0) {
        std::ostringstream m;
        m << "ocean tide requested to degree " << max_degree << " order " << max_order
          << "; the file carries degree and order " << header_.max_degree << ". Degree and order "
             "are separate limits and this refusal names both.";
        return odl::err(TidesError{"PERT-F-005", m.str()});
    }
    const std::array<double, 6> beta = doodson_arguments(args);
    const double u = header_.unit;

    std::ostringstream p;
    p << "FES2004 to degree " << max_degree << " order " << max_order << " of "
      << header_.max_degree << "; " << waves() << " waves; long period from FES2002 to ("
      << header_.long_period_max_degree << "," << header_.long_period_max_degree
      << "); equilibrium Om1/Om2 already in the file; atmospheric tide excluded";
    TideIncrements out{max_degree, TideSystem::ZeroTide,
                       ModelRecord{"ocean tide (FES2004)", "TN36-6 (6.15) with "
                                   "fes2004_Cnm-Snm.dat", p.str()}};

    for (const Row& r : rows_) {
        if (r.n > max_degree || r.m > max_order) continue;
        double theta = 0.0;
        for (int i = 0; i < 6; ++i) theta += r.doodson[static_cast<std::size_t>(i)] * beta[static_cast<std::size_t>(i)];
        const double ct = std::cos(theta), st = std::sin(theta);
        const double dc = ((r.cp + r.cm) * ct + (r.sp + r.sm) * st) * u;
        const double ds = ((r.sp - r.sm) * ct - (r.cp - r.cm) * st) * u;
        out.add(r.n, r.m, dc, ds);
    }
    // PERT-R-021.  FES2004 zeroes the retrograde zonal coefficients and DOUBLES
    // the prograde ones, so (6.15) gives the right dC_n0 and a spurious dS_n0.
    // Applying (6.15) uniformly and stopping there is the mistake; this is the
    // line that is easy to leave out and impossible to notice.
    for (int n = 0; n <= max_degree; ++n) out.add(n, 0, 0.0, -out.ds(n, 0));
    return out;
}

}  // namespace odl::tides
