#pragma once
// odl/estimation/dense.hpp — the small dense kernels of the estimator (SPEC-estimation.md EST-R-204, §5.1).
//
// Every matrix is n x n, ROW-MAJOR, and the kernels address the LOWER triangle unless they say they use both.  They are scale-agnostic by
// nature (DEM89's theorem is exactly that the unscaled Cholesky meets the scaled bound), which is why they are public: the closed-form tests
// of EST-A-203 address them directly.  They are NOT reachable from a NormalEquations except through the scaled matrix: the estimator has no
// path that factors an unscaled one (EST-R-201).
//
// THE ORDER OF OPERATIONS IS PART OF THE SPECIFICATION.  Each sum is accumulated by successive subtraction or addition in the order stated,
// in plain double, and the module is built with fused multiply-add contraction OFF, so the roundings are the ones the registered bounds
// (SPEC-estimation §3.4) and the registered predictions (tools/estimation_sizing.cpp replicates these loops; it was Python until group C9) refer to.

#include <cstddef>
#include <span>

namespace odl::estimation {

/// The outcome of a factorisation.  On breakdown `index` is the first row whose pivot `t` failed `t > 0`, and `pivot` is that `t` (it may be
/// zero, negative or NaN); the rows before `index` of the lower triangle hold the completed factor.
struct CholeskyResult {
    bool ok = true;
    std::size_t index = 0;
    double pivot = 0.0;
};

/// DEM89's Algorithm 2.1 on the lower triangle of the n x n matrix `a`, in place: for i = 0 … n-1, t = a(i,i) minus sum_{k<i} L(i,k)^2 by
/// successive subtraction in the order k = 0 … i-1; breakdown if !(t > 0); L(i,i) = sqrt(t); for j > i, L(j,i) = (a(j,i) minus
/// sum_{k<i} L(j,k) L(i,k) by successive subtraction) / L(i,i).
[[nodiscard]] CholeskyResult cholesky_lower(std::span<double> a, std::size_t n) noexcept;

/// Solves L z = b in place, L the lower triangle of `l`: ascending, each unknown by successive subtraction in the order k = 0 … i-1.
void forward_substitute(std::span<const double> l, std::size_t n, std::span<double> b) noexcept;

/// Solves L^T y = z in place: descending, each unknown by successive subtraction in the order k = n-1 … i+1.
void backward_substitute_transposed(std::span<const double> l, std::size_t n, std::span<double> z) noexcept;

/// The 1-norm of an n x n matrix, both triangles: the largest column sum of absolute values (sums ascending).
[[nodiscard]] double norm_one(std::span<const double> a, std::size_t n) noexcept;

/// y = A x for a matrix stored FULL (both triangles), each entry of y a sum ascending in the column index.
void multiply(std::span<const double> a, std::size_t n, std::span<const double> x, std::span<double> y) noexcept;

}  // namespace odl::estimation
