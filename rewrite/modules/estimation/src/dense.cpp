#include <odl/estimation/dense.hpp>

#include <cmath>

namespace odl::estimation {

CholeskyResult cholesky_lower(std::span<double> a, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        double t = a[i * n + i];
        for (std::size_t k = 0; k < i; ++k) t -= a[i * n + k] * a[i * n + k];
        if (!(t > 0.0)) return CholeskyResult{false, i, t};
        const double lii = std::sqrt(t);
        a[i * n + i] = lii;
        for (std::size_t j = i + 1; j < n; ++j) {
            double s = a[j * n + i];
            for (std::size_t k = 0; k < i; ++k) s -= a[j * n + k] * a[i * n + k];
            a[j * n + i] = s / lii;
        }
    }
    return CholeskyResult{};
}

void forward_substitute(std::span<const double> l, std::size_t n, std::span<double> b) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        double s = b[i];
        for (std::size_t k = 0; k < i; ++k) s -= l[i * n + k] * b[k];
        b[i] = s / l[i * n + i];
    }
}

void backward_substitute_transposed(std::span<const double> l, std::size_t n, std::span<double> z) noexcept {
    for (std::size_t ii = n; ii > 0; --ii) {
        const std::size_t i = ii - 1;
        double s = z[i];
        for (std::size_t k = n; k > i + 1; --k) s -= l[(k - 1) * n + i] * z[k - 1];
        z[i] = s / l[i * n + i];
    }
}

double norm_one(std::span<const double> a, std::size_t n) noexcept {
    double best = 0.0;
    for (std::size_t j = 0; j < n; ++j) {
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i) s += std::fabs(a[i * n + j]);
        if (s > best || std::isnan(s)) best = s;
    }
    return best;
}

void multiply(std::span<const double> a, std::size_t n, std::span<const double> x, std::span<double> y) noexcept {
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t j = 0; j < n; ++j) s += a[i * n + j] * x[j];
        y[i] = s;
    }
}

}  // namespace odl::estimation
