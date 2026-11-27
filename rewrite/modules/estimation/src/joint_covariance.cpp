#include <odl/estimation/joint_covariance.hpp>

#include <cmath>
#include <cstdio>

namespace odl::estimation {

namespace {

[[nodiscard]] std::string fmt(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.6g", v);
    return buf;
}

[[nodiscard]] EstimationRefusal refusal(const char* id, std::string text, std::string parameter = {}, double value = 0.0) {
    return EstimationRefusal{id, std::move(text), std::move(parameter), value};
}

[[nodiscard]] bool open_unit(double p) noexcept { return std::isfinite(p) && p > 0.0 && p < 1.0; }

[[nodiscard]] std::size_t index_of(const std::vector<std::string>& names, const std::string& name) {
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] == name) return i;
    return names.size();
}

}  // namespace

// ------------------------------------------------------------------------------------------------------------------ the critical values (EST-R-501, -507)

odl::Result<double, EstimationRefusal> chi2_2_critical(double p) {
    if (!open_unit(p)) return odl::err(refusal("EST-F-501", "the confidence level p = " + fmt(p) + " is not in the open interval (0, 1)", {}, p));
    return -2.0 * std::log1p(-p);
}

odl::Result<double, EstimationRefusal> two_f_2_nu_critical(double p, std::ptrdiff_t nu) {
    if (!open_unit(p)) return odl::err(refusal("EST-F-501", "the confidence level p = " + fmt(p) + " is not in the open interval (0, 1)", {}, p));
    if (nu < 1) return odl::err(refusal("EST-F-501", "the degrees of freedom nu = " + std::to_string(nu) + " is below 1", {}, static_cast<double>(nu)));
    const double n = static_cast<double>(nu);
    return n * std::expm1(-(2.0 / n) * std::log1p(-p));
}

odl::Result<double, EstimationRefusal> confidence_critical_value(ConfidenceMode mode, double p, std::ptrdiff_t nu) {
    return mode == ConfidenceMode::known_sigma0 ? chi2_2_critical(p) : two_f_2_nu_critical(p, nu);
}

odl::Result<double, EstimationRefusal> chi2_2_critical_from_tail(double alpha) {
    if (!open_unit(alpha)) return odl::err(refusal("EST-F-501", "the upper tail alpha = " + fmt(alpha) + " is not in the open interval (0, 1)", {}, alpha));
    return -2.0 * std::log(alpha);
}

odl::Result<double, EstimationRefusal> two_f_2_nu_critical_from_tail(double alpha, std::ptrdiff_t nu) {
    if (!open_unit(alpha)) return odl::err(refusal("EST-F-501", "the upper tail alpha = " + fmt(alpha) + " is not in the open interval (0, 1)", {}, alpha));
    if (nu < 1) return odl::err(refusal("EST-F-501", "the degrees of freedom nu = " + std::to_string(nu) + " is below 1", {}, static_cast<double>(nu)));
    const double n = static_cast<double>(nu);
    return n * std::expm1(-(2.0 / n) * std::log(alpha));
}

// ------------------------------------------------------------------------------------------------------------------ the ellipse (EST-R-502)

double ConfidenceEllipse2D::quadratic(std::array<double, 2> v) const noexcept {
    const double a = covariance[0], b = covariance[1], c = covariance[2];
    const double det = a * c - b * b;
    return (c * v[0] * v[0] - 2.0 * b * v[0] * v[1] + a * v[1] * v[1]) / det;
}

bool ConfidenceEllipse2D::contains(std::array<double, 2> point) const noexcept {
    return quadratic({point[0] - centre[0], point[1] - centre[1]}) <= level;
}

std::array<double, 2> ConfidenceEllipse2D::boundary_point(double t) const noexcept {
    const double cs = std::cos(angle), sn = std::sin(angle);
    const double u = semi_major * std::cos(t), w = semi_minor * std::sin(t);
    return {centre[0] + (cs * u - sn * w), centre[1] + (sn * u + cs * w)};
}

odl::Result<ConfidenceEllipse2D, EstimationRefusal> two_parameter_ellipse(const Solution& s, const std::string& name_i, const std::string& name_j, ConfidenceMode mode, double p,
                                                                          double known_sigma0) {
    if (!(std::isfinite(known_sigma0) && known_sigma0 > 0.0))
        return odl::err(refusal("EST-F-501", "the stated sigma_0 = " + fmt(known_sigma0) + " is not finite and positive", {}, known_sigma0));
    if (!open_unit(p)) return odl::err(refusal("EST-F-501", "the confidence level p = " + fmt(p) + " is not in the open interval (0, 1)", {}, p));
    const std::size_t n = s.names.size();
    const std::size_t i = index_of(s.names, name_i), j = index_of(s.names, name_j);
    if (i == n) return odl::err(refusal("EST-F-502", "the pair names '" + name_i + "', which the solution does not hold", name_i));
    if (j == n) return odl::err(refusal("EST-F-502", "the pair names '" + name_j + "', which the solution does not hold", name_j));
    if (i == j) return odl::err(refusal("EST-F-502", "the pair names the same parameter twice ('" + name_i + "')", name_i));
    if (s.eliminated[i]) return odl::err(refusal("EST-F-502", "'" + name_i + "' is eliminated (EST-R-209): it has no uncertainty, its row of Q is exactly zero", name_i));
    if (s.eliminated[j]) return odl::err(refusal("EST-F-502", "'" + name_j + "' is eliminated (EST-R-209): it has no uncertainty, its row of Q is exactly zero", name_j));

    double factor = known_sigma0 * known_sigma0;
    if (mode == ConfidenceMode::estimated_sigma0) {
        const auto s2 = s.sigma0_squared();
        if (!s2) return odl::err(refusal("EST-F-503", "the estimated-sigma_0 region needs sigma-hat_0^2: " + s2.error().message, {}, s2.error().value));
        factor = *s2;
    }
    const double a = factor * s.cofactor[i * n + i], b = factor * s.cofactor[i * n + j], c = factor * s.cofactor[j * n + j];
    const double det = a * c - b * b;
    if (!(std::isfinite(det) && det > 0.0 && a > 0.0 && c > 0.0))
        return odl::err(refusal("EST-F-504", "the 2 x 2 covariance block of (" + name_i + ", " + name_j + ") is degenerate: C_ii = " + fmt(a) + ", C_ij = " + fmt(b) + ", C_jj = " + fmt(c) +
                                                 ", det = " + fmt(det), {}, det));
    const auto level = confidence_critical_value(mode, p, s.degrees_of_freedom);
    if (!level) return odl::err(level.error());

    ConfidenceEllipse2D e;
    e.name_i = name_i;
    e.name_j = name_j;
    e.mode = mode;
    e.p = p;
    e.level = *level;
    e.centre = {s.x[i], s.x[j]};
    e.covariance = {a, b, c};
    const double h = 0.5 * (a - c);
    const double rad = std::sqrt(h * h + b * b);                     // no hypot: the plain operations scale exactly by a power of two
    const double lam_plus = 0.5 * (a + c) + rad;
    const double lam_minus = det / lam_plus;                         // the small eigenvalue without cancellation
    e.semi_major = std::sqrt(lam_plus * e.level);
    e.semi_minor = std::sqrt(lam_minus * e.level);
    e.angle = 0.5 * std::atan2(2.0 * b + 0.0, a - c);                // + 0.0 turns -0 into +0: the angle of a diagonal block is 0 or pi/2, never -pi/2
    return e;
}

// ------------------------------------------------------------------------------------------------------------------ the layout and the joint covariance (EST-R-503, -504)

odl::Result<JointParameterLayout, EstimationRefusal> JointParameterLayout::create(std::array<std::string, 6> state, std::vector<std::string> forces) {
    std::vector<std::string> all(state.begin(), state.end());
    for (auto& f : forces) all.push_back(f);
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].empty()) return odl::err(refusal("EST-F-505", "the layout's parameter " + std::to_string(i) + " has an empty name"));
        for (std::size_t j = 0; j < i; ++j)
            if (all[j] == all[i])
                return odl::err(refusal("EST-F-505", "the layout names '" + all[i] + "' twice (positions " + std::to_string(j) + " and " + std::to_string(i) + ")", all[i]));
    }
    JointParameterLayout l;
    l.state_names = std::move(state);
    l.force_names = std::move(forces);
    return l;
}

std::vector<std::string> JointParameterLayout::all() const {
    std::vector<std::string> out(state_names.begin(), state_names.end());
    for (const auto& f : force_names) out.push_back(f);
    return out;
}

odl::Result<JointCovariance, EstimationRefusal> joint_covariance(const Solution& s, const JointParameterLayout& layout) {
    const std::vector<std::string> want = layout.all();
    if (want.size() != s.names.size())
        return odl::err(refusal("EST-F-505", "the layout has " + std::to_string(want.size()) + " parameters (6 state + " + std::to_string(layout.force_names.size()) +
                                                 " force); the solution has " + std::to_string(s.names.size()), {}, static_cast<double>(s.names.size())));
    for (std::size_t k = 0; k < want.size(); ++k)
        if (want[k] != s.names[k])
            return odl::err(refusal("EST-F-505", "the solution's parameter " + std::to_string(k) + " is '" + s.names[k] + "'; the layout (state, then L3's registration order) has '" + want[k] + "' there",
                                    s.names[k]));
    const std::size_t n = s.names.size();
    JointCovariance jc;
    jc.names = s.names;
    jc.formal = s.cofactor;
    if (const auto s2 = s.sigma0_squared(); s2) {
        std::vector<double> sc(s.cofactor.size());
        for (std::size_t k = 0; k < sc.size(); ++k) sc[k] = *s2 * s.cofactor[k];
        jc.scaled = std::move(sc);
    }
    jc.correlation.assign(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) jc.correlation[i * n + j] = s.correlation(i, j);
    jc.eliminated = s.eliminated;
    return jc;
}

odl::Result<double, EstimationRefusal> JointCovariance::difference_sd(const std::string& i_name, const std::string& j_name, ConfidenceMode mode) const {
    const std::size_t n = names.size();
    const std::size_t i = index_of(names, i_name), j = index_of(names, j_name);
    if (i == n) return odl::err(refusal("EST-F-502", "the difference names '" + i_name + "', which the covariance does not hold", i_name));
    if (j == n) return odl::err(refusal("EST-F-502", "the difference names '" + j_name + "', which the covariance does not hold", j_name));
    if (i == j) return odl::err(refusal("EST-F-502", "the difference names the same parameter twice ('" + i_name + "')", i_name));
    if (eliminated[i] || eliminated[j]) return odl::err(refusal("EST-F-502", "a parameter of the difference is eliminated: it has no uncertainty", eliminated[i] ? i_name : j_name));
    const std::vector<double>* C = &formal;
    if (mode == ConfidenceMode::estimated_sigma0) {
        if (!scaled) return odl::err(refusal("EST-F-503", "the scaled covariance is absent: sigma-hat_0^2 is unavailable (nu <= 0 or no residual square sum)"));
        C = &*scaled;
    }
    const double v = (*C)[i * n + i] - 2.0 * (*C)[i * n + j] + (*C)[j * n + j];
    if (!(std::isfinite(v) && v >= 0.0)) return odl::err(refusal("EST-F-504", "the variance of the difference " + i_name + " - " + j_name + " is " + fmt(v) + ", not a non-negative number", {}, v));
    return std::sqrt(v);
}

}  // namespace odl::estimation
