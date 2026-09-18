#pragma once
// odl/gravity/degree.hpp — Degree and Order, which are both small integers and
// are not interchangeable.
//
// SPEC-gravity GRAV-R-051: passing (order, degree) where (degree, order) is
// meant MUST NOT COMPILE.  Two `int` parameters next to each other is an
// invitation; two distinct opaque types is a compile error.  This is the same
// move as the frame living in the type rather than in a field.

#include <odl/core/result.hpp>

#include <cstdint>

namespace odl::gravity {

using GravityError = odl::Diagnostic;

/// The maximum degree the model carries (TN36-6 §6.1: EGM2008 has additional
/// coefficients to degree 2190).
inline constexpr int kEgm2008MaxDegree = 2190;
/// The maximum ORDER, which is not the same number and never was.
inline constexpr int kEgm2008MaxOrder = 2159;

class Degree {
public:
    Degree() = delete;
    [[nodiscard]] static odl::Result<Degree, GravityError> of(int n);
    [[nodiscard]] constexpr int value() const noexcept { return n_; }

private:
    explicit constexpr Degree(int n) noexcept : n_(n) {}
    int n_;
};

class Order {
public:
    Order() = delete;
    [[nodiscard]] static odl::Result<Order, GravityError> of(int m);
    [[nodiscard]] constexpr int value() const noexcept { return m_; }

private:
    explicit constexpr Order(int m) noexcept : m_(m) {}
    int m_;
};

}  // namespace odl::gravity
