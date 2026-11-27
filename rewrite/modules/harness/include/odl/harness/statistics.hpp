#pragma once
// odl/harness/statistics.hpp — the campaign statistics of L8 step 1, each NAMING ITS FORMULA (plan §4 rule 3; plan/subplan_L8/L8-1.md, registered 2026-11-27).
//
//   orbit residual     S_N      = sqrt( (1/N)       sum_{i=1..N} ||q_i - p_i||^2 )
//                      S_{N-p}  = sqrt( (1/(N - p)) sum_{i=1..N} ||q_i - p_i||^2 )     p = the number of fitted parameters
//                      S_N/sqrt(3); the radial / along-track / cross-track RMS (each sqrt(mean of that component^2)); the largest epoch residual
//   block summary      m_b = (1/n_b) sum x_i,   s_b = sqrt( (1/(n_b - 1)) sum (x_i - m_b)^2 )     n_b >= 2 (Bessel)
//   block separation   d_pooled = |m_1 - m_2| / sqrt( ((n_1 - 1) s_1^2 + (n_2 - 1) s_2^2) / (n_1 + n_2 - 2) )      (the frozen B-*-pooled definition)
//                      t_sem    = |m_1 - m_2| / sqrt( s_1^2/n_1 + s_2^2/n_2 )                                      (the frozen B-*-sem definition)
//   constellation      median(S_N) (the mean of the two middle values for an even count) and max(S_N)
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace odl::harness {

using HarnessError = odl::Diagnostic;

/// A value that carries the name of the formula it was computed with.
struct NamedStatistic {
    std::string name;
    std::string formula;
    double value = 0.0;
};

/// One epoch of an orbit fit: the model minus the file in the ITRS (metres), the rotation from the ITRS to the GCRS, and the model's GCRS state (metres, metres per second), from which the RTN frame is built.
struct EpochResidual {
    Vec3 q_minus_p_itrs_m;
    Mat3 itrs_to_gcrs;
    Vec3 position_gcrs_m;
    Vec3 velocity_gcrs_m;
};

struct OrbitResidualStats {
    std::size_t n = 0;                 ///< N, the epochs
    std::size_t parameters = 0;        ///< p
    double s_n_m = 0.0;                ///< S_N
    double s_n_minus_p_m = 0.0;        ///< S_{N-p}
    double s_n_over_sqrt3_m = 0.0;     ///< S_N / sqrt(3)
    double rms_radial_m = 0.0, rms_along_m = 0.0, rms_cross_m = 0.0;
    double max_epoch_m = 0.0;
};

struct BlockSummary {
    std::string block;
    std::size_t n = 0;
    double mean = 0.0;                 ///< m_b
    double sd = 0.0;                   ///< s_b, Bessel
};

struct BlockSeparation {
    std::string block_1, block_2;
    double d_pooled = 0.0;
    double t_sem = 0.0;
};

struct ConstellationSummary {
    std::size_t count = 0;             ///< satellites
    std::size_t eclipse_free_count = 0;
    double median_sn_m = 0.0;
    double max_sn_m = 0.0;
};

/// EpochResidual list -> OrbitResidualStats.  Refuses (HARN-F-001) an empty list or `parameters >= N` (S_{N-p} undefined), and a non-finite input; (HARN-F-002) a state with a zero position or a zero angular momentum (no RTN frame).
[[nodiscard]] odl::Result<OrbitResidualStats, HarnessError> compute_orbit_residuals(const std::vector<EpochResidual>& epochs, std::size_t parameters);

/// Mean and Bessel standard deviation of a block's values; refuses (HARN-F-003) `n_b < 2` or a non-finite value.
[[nodiscard]] odl::Result<BlockSummary, HarnessError> compute_block_summary(const std::string& block, const std::vector<double>& values);

/// Both separations of two blocks; refuses (HARN-F-004) `n_1 < 2` or `n_2 < 2`, and a denominator that is zero or not finite.
[[nodiscard]] odl::Result<BlockSeparation, HarnessError> compute_block_separation(const BlockSummary& a, const BlockSummary& b);

/// The median (the mean of the two middle values when the count is even); refuses (HARN-F-005) an empty list or a non-finite value.
[[nodiscard]] odl::Result<double, HarnessError> compute_median(std::vector<double> values);

/// median and max of the S_N values; `eclipse_free` marks (by a same-size flag list) the satellites counted in `eclipse_free_count`.  Refuses (HARN-F-005/006) an empty list or a flag list of another size.
[[nodiscard]] odl::Result<ConstellationSummary, HarnessError> compute_constellation_summary(const std::vector<double>& s_n_m, const std::vector<bool>& eclipse_free);

}  // namespace odl::harness
