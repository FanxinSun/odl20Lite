#pragma once
// rkf78_coefficients.hpp — GENERATED.  Do not edit.
//
// Produced by tools/rk_coefficients.py from Fehlberg, NASA TR R-287 (1968),
// Table X (report p.65), sha256 5553a2a3eb53785a461762cc2b29428015f1b32c3ad0a5cb57f85a421256a0c8
//
// READ FROM THE PAGE IMAGE, NOT THE TEXT LAYER.  The report is a 1968 scan and
// pdftotext returns noise for its mathematics; the images at 300 dpi are exact
// and give the coefficients as RATIONALS.  See the manifest entry's verify_note.
//
// AND THE TRANSCRIPTION IS PROVED, NOT TRUSTED.  In exact rational arithmetic:
//   row-sum consistency   13/13 rows, sum_j beta_ij = alpha_i
//   c    through order 7  85 order conditions, 0 violated
//   chat through order 8  200 order conditions, 0 violated
//   c    at order 8       40 of 115 violated
//
// That last line is an INDEPENDENT CROSS-CHECK that never touches the digits:
// there are 115 rooted trees of order 8, exactly the range of
// Fehlberg's error coefficients T_v, and his Section XV says in PROSE that RK7(8)
// has "only 40 non-zero error coefficients T_v".  Prose on one page and
// mathematics applied to a table on another agree.
//
// THE ESTIMATOR IS IDENTICALLY ZERO ON A QUADRATURE PROBLEM.  alpha_0 = alpha_11 = 0
// and alpha_10 = alpha_12 = 1, and the truncation term (134) is
//     TE = (41/840)(f0 + f10 - f11 - f12) h,
// so for any right-hand side depending on x alone the four evaluations cancel in
// pairs and the estimate is not small but ZERO.  See SPEC-integrators; the
// controller is blind there, not merely optimistic.

#include <array>

namespace odl::integrators::rkf78 {

inline constexpr int kStages = 13;

inline constexpr std::array<double, kStages> kAlpha = {
    0, 0.07407407407407407, 0.1111111111111111, 0.16666666666666666, 0.41666666666666669, 0.5, 0.83333333333333337, 0.16666666666666666, 0.66666666666666663, 0.33333333333333331, 1, 0, 1
};

/// Row-major, lower triangular; entries above the diagonal are zero.
inline constexpr std::array<std::array<double, kStages>, kStages> kBeta = {{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.07407407407407407, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.027777777777777776, 0.083333333333333329, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.041666666666666664, 0, 0.125, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.41666666666666669, 0, -1.5625, 1.5625, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0.050000000000000003, 0, 0, 0.25, 0.20000000000000001, 0, 0, 0, 0, 0, 0, 0, 0},
    {-0.23148148148148148, 0, 0, 1.1574074074074074, -2.4074074074074074, 2.3148148148148149, 0, 0, 0, 0, 0, 0, 0},
    {0.10333333333333333, 0, 0, 0, 0.27111111111111114, -0.22222222222222221, 0.014444444444444444, 0, 0, 0, 0, 0, 0},
    {2, 0, 0, -8.8333333333333339, 15.644444444444444, -11.888888888888889, 0.74444444444444446, 3, 0, 0, 0, 0, 0},
    {-0.84259259259259256, 0, 0, 0.21296296296296297, -7.2296296296296294, 5.7592592592592595, -0.31666666666666665, 2.8333333333333335, -0.083333333333333329, 0, 0, 0, 0},
    {0.58121951219512191, 0, 0, -2.0792682926829267, 4.3863414634146345, -3.6707317073170733, 0.52024390243902441, 0.54878048780487809, 0.27439024390243905, 0.43902439024390244, 0, 0, 0},
    {0.014634146341463415, 0, 0, 0, 0, -0.14634146341463414, -0.014634146341463415, -0.073170731707317069, 0.073170731707317069, 0.14634146341463414, 0, 0, 0},
    {-0.43341463414634146, 0, 0, -2.0792682926829267, 4.3863414634146345, -3.524390243902439, 0.53487804878048784, 0.62195121951219512, 0.20121951219512196, 0.29268292682926828, 0, 1, 0},
}};

/// The propagating 7th-order weights.
inline constexpr std::array<double, kStages> kC = {
    0.04880952380952381, 0, 0, 0, 0, 0.32380952380952382, 0.25714285714285712, 0.25714285714285712, 0.03214285714285714, 0.03214285714285714, 0.04880952380952381, 0, 0
};

/// The 8th-order companion, used only for the error estimate.
inline constexpr std::array<double, kStages> kCHat = {
    0, 0, 0, 0, 0, 0.32380952380952382, 0.25714285714285712, 0.25714285714285712, 0.03214285714285714, 0.03214285714285714, 0, 0.04880952380952381, 0.04880952380952381
};

/// (134): TE = kErrorWeight * (f0 + f10 - f11 - f12) * h
inline constexpr double kErrorWeight = 0.04880952380952381;
inline constexpr int kOrder = 7;          ///< the order that PROPAGATES
inline constexpr int kEstimatorOrder = 8;
inline constexpr int kOrder8Violations = 40;

}  // namespace odl::integrators::rkf78
