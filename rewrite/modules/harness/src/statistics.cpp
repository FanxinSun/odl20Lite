#include <odl/harness/statistics.hpp>

#include <algorithm>
#include <cmath>

namespace odl::harness {

namespace {

[[nodiscard]] bool finite3(const Vec3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }

}  // namespace

odl::Result<OrbitResidualStats, HarnessError> compute_orbit_residuals(const std::vector<EpochResidual>& epochs, std::size_t parameters) {
    const std::size_t n = epochs.size();
    if (n == 0) return odl::err(HarnessError{"HARN-F-001", "orbit residuals: no epochs"});
    if (parameters >= n) return odl::err(HarnessError{"HARN-F-001", "orbit residuals: " + std::to_string(parameters) + " parameters against " + std::to_string(n) + " epochs: S_{N-p} is undefined"});
    double sum = 0.0, rr = 0.0, tt = 0.0, nn = 0.0, mx = 0.0;
    for (const EpochResidual& e : epochs) {
        if (!finite3(e.q_minus_p_itrs_m) || !finite3(e.position_gcrs_m) || !finite3(e.velocity_gcrs_m)) return odl::err(HarnessError{"HARN-F-001", "orbit residuals: a non-finite input"});
        const double n2 = e.q_minus_p_itrs_m.dot(e.q_minus_p_itrs_m);
        sum += n2;
        mx = std::max(mx, std::sqrt(n2));
        const double rn = e.position_gcrs_m.norm();
        const Vec3 h = e.position_gcrs_m.cross(e.velocity_gcrs_m);
        const double hn = h.norm();
        if (!(rn > 0.0) || !(hn > 0.0)) return odl::err(HarnessError{"HARN-F-002", "orbit residuals: a state with no radial or no angular-momentum direction"});
        const Vec3 radial = (1.0 / rn) * e.position_gcrs_m, normal = (1.0 / hn) * h, along = normal.cross(radial);
        const Vec3 d = e.itrs_to_gcrs.apply(e.q_minus_p_itrs_m);       // the residual in the GCRS
        rr += d.dot(radial) * d.dot(radial);
        tt += d.dot(along) * d.dot(along);
        nn += d.dot(normal) * d.dot(normal);
    }
    const double nd = static_cast<double>(n);
    OrbitResidualStats s;
    s.n = n;
    s.parameters = parameters;
    s.s_n_m = std::sqrt(sum / nd);
    s.s_n_minus_p_m = std::sqrt(sum / (nd - static_cast<double>(parameters)));
    s.s_n_over_sqrt3_m = s.s_n_m / std::sqrt(3.0);
    s.rms_radial_m = std::sqrt(rr / nd);
    s.rms_along_m = std::sqrt(tt / nd);
    s.rms_cross_m = std::sqrt(nn / nd);
    s.max_epoch_m = mx;
    return s;
}

odl::Result<BlockSummary, HarnessError> compute_block_summary(const std::string& block, const std::vector<double>& values) {
    if (values.size() < 2) return odl::err(HarnessError{"HARN-F-003", "block '" + block + "': n_b = " + std::to_string(values.size()) + " < 2: no Bessel standard deviation"});
    double sum = 0.0;
    for (double x : values) {
        if (!std::isfinite(x)) return odl::err(HarnessError{"HARN-F-003", "block '" + block + "': a non-finite value"});
        sum += x;
    }
    const double nb = static_cast<double>(values.size()), mean = sum / nb;
    double ss = 0.0;
    for (double x : values) ss += (x - mean) * (x - mean);
    BlockSummary b;
    b.block = block;
    b.n = values.size();
    b.mean = mean;
    b.sd = std::sqrt(ss / (nb - 1.0));
    return b;
}

odl::Result<BlockSeparation, HarnessError> compute_block_separation(const BlockSummary& a, const BlockSummary& b) {
    if (a.n < 2 || b.n < 2) return odl::err(HarnessError{"HARN-F-004", "block separation: both blocks need n >= 2"});
    const double n1 = static_cast<double>(a.n), n2 = static_cast<double>(b.n);
    const double pooled = std::sqrt(((n1 - 1.0) * a.sd * a.sd + (n2 - 1.0) * b.sd * b.sd) / (n1 + n2 - 2.0));
    const double sem = std::sqrt(a.sd * a.sd / n1 + b.sd * b.sd / n2);
    if (!(pooled > 0.0) || !std::isfinite(pooled) || !(sem > 0.0) || !std::isfinite(sem)) return odl::err(HarnessError{"HARN-F-004", "block separation: a zero or non-finite denominator"});
    BlockSeparation s;
    s.block_1 = a.block;
    s.block_2 = b.block;
    s.d_pooled = std::fabs(a.mean - b.mean) / pooled;
    s.t_sem = std::fabs(a.mean - b.mean) / sem;
    return s;
}

odl::Result<double, HarnessError> compute_median(std::vector<double> values) {
    if (values.empty()) return odl::err(HarnessError{"HARN-F-005", "median of no values"});
    for (double x : values)
        if (!std::isfinite(x)) return odl::err(HarnessError{"HARN-F-005", "median: a non-finite value"});
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    return n % 2 == 1 ? values[n / 2] : 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

odl::Result<ConstellationSummary, HarnessError> compute_constellation_summary(const std::vector<double>& s_n_m, const std::vector<bool>& eclipse_free) {
    if (s_n_m.empty()) return odl::err(HarnessError{"HARN-F-005", "constellation summary of no satellites"});
    if (eclipse_free.size() != s_n_m.size()) return odl::err(HarnessError{"HARN-F-006", "constellation summary: the eclipse flags and the S_N values differ in count"});
    auto med = compute_median(s_n_m);
    if (!med) return odl::err(med.error());
    ConstellationSummary c;
    c.count = s_n_m.size();
    c.median_sn_m = *med;
    c.max_sn_m = *std::max_element(s_n_m.begin(), s_n_m.end());
    for (bool f : eclipse_free)
        if (f) ++c.eclipse_free_count;
    return c;
}

}  // namespace odl::harness
