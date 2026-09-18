#pragma once
// odl/tides/pole.hpp — the solid Earth pole tide and the ocean pole tide.
//
// Both are the centrifugal effect of polar motion, TN36-6 §6.4 and §6.5, and
// both are computed from their constants rather than from the shorthand the
// Conventions print — with the printed shorthand as the test (PERT-A-005,
// PERT-A-006).
//
// TWO NUMBERS IN §6.4 DISAGREE WITH EACH OTHER.  It prints k2 = 0.3077 + 0.0036i
// and a cross-term ratio of 0.0115; Im(k2)/Re(k2) is 0.011700, and the printed
// ratio implies Im(k2) = 0.003539.  1.7 % of the imaginary part, 0.02 % of
// ΔC̄21.  This module follows k2 — the constant is the input and the ratio is a
// convenience derived from it — and PERT-A-005 reports both rather than
// choosing quietly.

#include <odl/core/result.hpp>
#include <odl/tides/increments.hpp>
#include <odl/tides/wobble.hpp>

#include <string>

namespace odl::tides {

class SolidEarthPoleTide {
public:
    SolidEarthPoleTide() = delete;

    [[nodiscard]] static odl::Result<TideIncrements, TidesError> increments(const Wobble& w);

    /// TN36-6 §6.4, appropriate to the polar tide.
    static constexpr double kK2Real = 0.3077;
    static constexpr double kK2Imag = 0.0036;
    /// The ratio §6.4 PRINTS, which is not Im(k2)/Re(k2).  Carried so that the
    /// test can state the disagreement rather than hide it.
    static constexpr double kPrintedCrossTermRatio = 0.0115;
    /// The leading coefficient §6.4 prints, per second of arc.
    static constexpr double kPrintedLeadingPerArcsec = -1.333e-9;

    /// Ω² a_E³ k2R / (GM √15), the coefficient of m1 in ΔC̄21 for m in RADIANS.
    [[nodiscard]] static double leading_coefficient_per_radian() noexcept;
};

/// The self-consistent equilibrium model of TN36-6 §6.5, with the coefficients
/// Desai distributes to degree and order 360.
class OceanPoleTide {
public:
    [[nodiscard]] static odl::Result<OceanPoleTide, TidesError>
    load(const std::string& path, const std::string& cache_root);

    /// PERT-R-033: the truncation degree is a parameter and is recorded, and the
    /// module reports the variance fraction it retains rather than leaving the
    /// reader to the figure.
    [[nodiscard]] odl::Result<TideIncrements, TidesError>
    increments(const Wobble& w, int max_degree) const;

    /// THE FRACTION OF WHAT, EXACTLY.  TN36-6 §6.5 says degree 2 provides
    /// "approximately 90% of the variance of the OCEAN POLE TIDE POTENTIAL" and
    /// degree 10 approximately 99%.  That is the R_n-weighted variance,
    /// sum_m R_n^2 (A^2 + B^2), not the raw coefficient variance: the raw one
    /// gives 75.8% and 92.7% at those degrees and would have been reported as
    /// though it were the Conventions' number.  Both are exposed, with their
    /// formulas in their names, because SPEC-template.md §8 requires a statistic
    /// to carry the formula it was computed by.
    [[nodiscard]] double potential_variance_fraction(int degree) const noexcept;
    [[nodiscard]] double coefficient_variance_fraction(int degree) const noexcept;

    [[nodiscard]] int file_max_degree() const noexcept { return file_max_degree_; }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] double r_n(int n) const noexcept;

    /// TN36-6 §6.5.
    static constexpr double kGammaReal = 0.6870;
    static constexpr double kGammaImag = 0.0036;
    static constexpr double kSeaWaterDensity = 1025.0;   ///< kg m^-3

private:
    OceanPoleTide() = default;
    struct Row { int n, m; double a_re, b_re, a_im, b_im; };
    std::vector<Row> rows_data_;
    std::vector<double> variance_by_degree_;
    int file_max_degree_ = 0;
    std::size_t rows_ = 0;
    std::string path_;
};

}  // namespace odl::tides
