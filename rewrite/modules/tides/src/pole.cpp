#include <odl/tides/pole.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <sstream>

namespace odl::tides {

namespace {

// TN36-1's numerical standards.  PERT-R-009: these are NOT EGM2008's scaling
// pair — a_E is 6378136.6 m here against EGM2008's 6378136.3 m, and GM is the
// TCG-compatible value — and the tide formulae are written in these.
constexpr double kOmega = 7.292115146706979e-5;   ///< rad/s
constexpr double kAe = 6378136.6;                 ///< m
constexpr double kGm = 3.986004418e14;            ///< m^3/s^2
constexpr double kGe = 9.7803278;                 ///< m/s^2
constexpr double kBigG = 6.67428e-11;             ///< m^3 kg^-1 s^-2
constexpr double kArcsec = std::numbers::pi / (180.0 * 3600.0);

/// TN36-6 §6.5's load deformation coefficients.  Above degree 6 the Conventions
/// give no value; the module uses the degree-6 one and says so, because the
/// alternative is to stop at 6 and lose the continental boundaries §6.5 says
/// need high degree.
double load_deformation(int n) noexcept {
    switch (n) {
        case 2: return -0.3075;
        case 3: return -0.195;
        case 4: return -0.132;
        case 5: return -0.1032;
        case 6: return -0.0892;
        default: return -0.0892;
    }
}

}  // namespace

double SolidEarthPoleTide::leading_coefficient_per_radian() noexcept {
    return kOmega * kOmega * kAe * kAe * kAe * kK2Real / (kGm * std::sqrt(15.0));
}

odl::Result<TideIncrements, TidesError> SolidEarthPoleTide::increments(const Wobble& w) {
    // ΔC̄21 - iΔS̄21 = -K (k2R + i k2I)(m1 - i m2)  with K = Ω² a_E³ /(GM √15),
    // so ΔC̄21 = -K(k2R m1 + k2I m2) and ΔS̄21 = -K(k2R m2 - k2I m1).
    const double K = kOmega * kOmega * kAe * kAe * kAe / (kGm * std::sqrt(15.0));
    const double dc = -K * (kK2Real * w.m1_rad() + kK2Imag * w.m2_rad());
    const double ds = -K * (kK2Real * w.m2_rad() - kK2Imag * w.m1_rad());

    std::ostringstream p;
    p.precision(10);
    p << "k2 = " << kK2Real << " + " << kK2Imag << "i; m1 = " << w.m1_arcsec()
      << "\", m2 = " << w.m2_arcsec() << "\"";
    // The solid Earth pole tide is a deformation of the SOLID Earth by the
    // centrifugal potential. It carries no permanent part, so it is neutral
    // about the tide system and takes the zero-tide label the conventional
    // field uses, which is what lets it sum with the solid Earth tide.
    TideIncrements out{2, TideSystem::ZeroTide,
                       ModelRecord{"solid Earth pole tide", "TN36-6 §6.4", p.str()}};
    out.add(2, 1, dc, ds);
    return out;
}

odl::Result<OceanPoleTide, TidesError>
OceanPoleTide::load(const std::string& path, const std::string& cache_root) {
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(fs::path(path), ec);
    const fs::path root = fs::weakly_canonical(fs::path(cache_root), ec);
    const auto rel = canonical.lexically_relative(root);
    if (root.empty() || rel.empty() || *rel.begin() == "..") {
        std::ostringstream m;
        m << "the ocean pole tide coefficients are outside the manifest cache and will not be "
             "read.\n  requested  " << canonical.string() << "\n  cache root " << root.string();
        return odl::err(TidesError{"PERT-F-010", m.str()});
    }
    std::ifstream f(canonical);
    if (!f) return odl::err(TidesError{"PERT-F-010", "cannot open " + canonical.string()});

    OceanPoleTide out;
    out.path_ = canonical.string();
    std::string line;
    std::getline(f, line);                     // the column heading
    while (std::getline(f, line)) {
        std::istringstream s(line);
        Row r{};
        if (!(s >> r.n >> r.m >> r.a_re >> r.b_re >> r.a_im >> r.b_im)) continue;
        out.file_max_degree_ = std::max(out.file_max_degree_, r.n);
        out.rows_data_.push_back(r);
    }
    out.rows_ = out.rows_data_.size();
    if (out.rows_ < 1000 || out.file_max_degree_ != 360) {
        std::ostringstream m;
        m << "the ocean pole tide file is not shaped as TN36-6 §6.5 describes: " << out.rows_
          << " rows to degree " << out.file_max_degree_
          << ", where the Conventions distribute the coefficients to degree and order 360.";
        return odl::err(TidesError{"PERT-F-011", m.str()});
    }
    out.variance_by_degree_.assign(static_cast<std::size_t>(out.file_max_degree_) + 1, 0.0);
    for (const Row& r : out.rows_data_) {
        out.variance_by_degree_[static_cast<std::size_t>(r.n)] +=
            r.a_re * r.a_re + r.b_re * r.b_re + r.a_im * r.a_im + r.b_im * r.b_im;
    }
    return out;
}

double OceanPoleTide::r_n(int n) const noexcept {
    // TN36-6 (6.23b).
    return kOmega * kOmega * kAe * kAe * kAe * kAe / kGm
         * (4.0 * std::numbers::pi * kBigG * kSeaWaterDensity / kGe)
         * ((1.0 + load_deformation(n)) / (2.0 * n + 1.0));
}

namespace {
double fraction(const std::vector<double>& by_degree, int nmax, int degree,
                bool weight_by_rn, const OceanPoleTide& self) noexcept {
    double kept = 0.0, total = 0.0;
    for (int n = 2; n <= nmax; ++n) {
        const double w = weight_by_rn ? self.r_n(n) * self.r_n(n) : 1.0;
        const double v = by_degree[static_cast<std::size_t>(n)] * w;
        total += v;
        if (n <= degree) kept += v;
    }
    return total > 0.0 ? kept / total : 0.0;
}
}  // namespace

double OceanPoleTide::potential_variance_fraction(int degree) const noexcept {
    return fraction(variance_by_degree_, file_max_degree_, degree, true, *this);
}

double OceanPoleTide::coefficient_variance_fraction(int degree) const noexcept {
    return fraction(variance_by_degree_, file_max_degree_, degree, false, *this);
}

odl::Result<TideIncrements, TidesError>
OceanPoleTide::increments(const Wobble& w, int max_degree) const {
    if (max_degree < 2 || max_degree > file_max_degree_) {
        std::ostringstream m;
        m << "ocean pole tide requested to degree " << max_degree << "; the file carries 2 to "
          << file_max_degree_ << ".";
        return odl::err(TidesError{"PERT-F-005", m.str()});
    }
    // TN36-6 (6.23a), with m in RADIANS.  §6.5's printed result is per ARCSECOND
    // and the two appear on the same page; PERT-R-008 is why the interface names
    // its unit.
    const double u = w.m1_rad() * kGammaReal + w.m2_rad() * kGammaImag;
    const double v = w.m2_rad() * kGammaReal - w.m1_rad() * kGammaImag;

    std::ostringstream p;
    p.precision(10);
    p << "degree " << max_degree << " of " << file_max_degree_ << ", retaining "
      << potential_variance_fraction(max_degree) * 100.0 << "% of the potential variance; m1 = "
      << w.m1_arcsec() << "\", m2 = " << w.m2_arcsec() << "\"";
    TideIncrements out{max_degree, TideSystem::ZeroTide,
                       ModelRecord{"ocean pole tide (Desai self-consistent equilibrium)",
                                   "TN36-6 §6.5 with desaiscopolecoef.txt", p.str()}};
    for (const Row& r : rows_data_) {
        // PERT-R-034: degree 1 is not exported.  The Conventions say the
        // degree-1 terms must not be used for station displacement; nothing here
        // computes one, and the way to keep it that way is not to hand them out.
        if (r.n < 2 || r.n > max_degree) continue;
        const double R = r_n(r.n);
        out.add(r.n, r.m, R * (r.a_re * u + r.a_im * v), R * (r.b_re * u + r.b_im * v));
    }
    return out;
}

}  // namespace odl::tides
