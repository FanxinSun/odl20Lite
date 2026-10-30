#pragma once
// dual.hpp — forward-mode automatic differentiation for the TESTS of L7 step 3 (the manager's point (b) of 2026-10-31: it stays in tests/, not in modules/estimation).
// A `Dual` is a value and its gradient with respect to up to kMaxParameters parameters; the NIST models are written ONCE, as templates over the number type, and evaluated on `Dual`
// to give the Jacobian and on `double` to give the value, so no hand-differentiated Jacobian is a source of doubt.  The Jacobians are checked against central differences
// in the tests (the check this type's own tests make: a derivative that agrees with a difference quotient, at several points of several models).
#include <array>
#include <cmath>
#include <cstddef>

namespace odl::estimation::fx {

inline constexpr std::size_t kMaxParameters = 9;   // ENSO has nine

struct Dual {
    double v = 0.0;
    std::array<double, kMaxParameters> g{};

    Dual() = default;
    Dual(double value) : v(value) {}                                   // NOLINT: a constant
    [[nodiscard]] static Dual variable(double value, std::size_t index) {
        Dual d(value);
        d.g[index] = 1.0;
        return d;
    }
};

inline Dual operator-(const Dual& a) {
    Dual r(-a.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = -a.g[i];
    return r;
}
inline Dual operator+(const Dual& a, const Dual& b) {
    Dual r(a.v + b.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = a.g[i] + b.g[i];
    return r;
}
inline Dual operator-(const Dual& a, const Dual& b) {
    Dual r(a.v - b.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = a.g[i] - b.g[i];
    return r;
}
inline Dual operator*(const Dual& a, const Dual& b) {
    Dual r(a.v * b.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = a.g[i] * b.v + a.v * b.g[i];
    return r;
}
inline Dual operator/(const Dual& a, const Dual& b) {
    Dual r(a.v / b.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = (a.g[i] - r.v * b.g[i]) / b.v;
    return r;
}
inline Dual exp(const Dual& a) {
    Dual r(std::exp(a.v));
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = r.v * a.g[i];
    return r;
}
inline Dual log(const Dual& a) {
    Dual r(std::log(a.v));
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = a.g[i] / a.v;
    return r;
}
inline Dual sin(const Dual& a) {
    Dual r(std::sin(a.v));
    const double c = std::cos(a.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = c * a.g[i];
    return r;
}
inline Dual cos(const Dual& a) {
    Dual r(std::cos(a.v));
    const double s = -std::sin(a.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = s * a.g[i];
    return r;
}
inline Dual atan(const Dual& a) {
    Dual r(std::atan(a.v));
    const double d = 1.0 / (1.0 + a.v * a.v);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = d * a.g[i];
    return r;
}
/// a^p for a constant exponent p (a > 0 or p integral).
inline Dual pow(const Dual& a, double p) {
    Dual r(std::pow(a.v, p));
    const double d = p * std::pow(a.v, p - 1.0);
    for (std::size_t i = 0; i < kMaxParameters; ++i) r.g[i] = d * a.g[i];
    return r;
}
/// a^e for a dual exponent, a > 0: exp(e log a).
inline Dual pow(const Dual& a, const Dual& e) { return exp(e * log(a)); }

// the same functions on double, so that a model written as a template over T finds std:: or these by the unqualified call
using std::atan;
using std::cos;
using std::exp;
using std::log;
using std::pow;
using std::sin;

}  // namespace odl::estimation::fx
