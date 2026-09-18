#pragma once
// odl/gravity/coefficients.hpp — the published coefficient set, and the four
// ways its file is not shaped the way a reader would assume.
//
// Measured from EGM2008_to2190_TideFree itself, not assumed (PROVENANCE §14.4):
//
//   * 2 401 333 records — the full triangle to (2190, 2190) less three.
//   * DEGREES 0 AND 1 ARE ABSENT.  Degree 0 is the two-body term, carried by
//     GM; degree 1 vanishes because the origin is the centre of mass.  A reader
//     that assumes records start at n = 0 misindexes everything after.
//   * THE FILE IS PADDED TO A FULL TRIANGLE WITH EXPLICIT ZEROS, so the model's
//     true order cannot be read off its last record, which is (2190, 2190) and
//     is zero.
//   * The highest order carrying a non-zero coefficient is 2159 at ODD degrees
//     above 2159 but 2158 at EVEN ones — the parity coupling of the
//     ellipsoidal-to-spherical conversion behind EGM2008, which couples degrees
//     of equal parity and preserves order.
//
// The exponent marker is FORTRAN `D`.  A reader that only accepts `E` reads
// nothing at all, which at least fails loudly; one that accepts `D` by treating
// the rest of the field as garbage would not.

#include <odl/core/result.hpp>
#include <odl/gravity/degree.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace odl::gravity {

/// TN36-1 and TN36-6 §6.2.2.  Which convention C20 follows for the permanent
/// tide.  GRAV-R-008: this travels with the field rather than being documented,
/// because L2 step 3 must remove the permanent part from the tide variations
/// exactly when the background field is zero-tide (TN36-6 (6.13)).
enum class TideSystem { TideFree, ZeroTide };

[[nodiscard]] constexpr const char* name_of(TideSystem t) noexcept {
    return t == TideSystem::TideFree ? "conventional tide free" : "zero tide";
}

/// The coefficients as published, before any conventional substitution.
class CoefficientSet {
public:
    /// GRAV-R-010/-011/-012/-013/-014.  `path` must be inside the manifest
    /// cache; `cache_root` is the directory the fetcher owns.
    [[nodiscard]] static odl::Result<CoefficientSet, GravityError>
    load_egm2008(const std::string& path, const std::string& cache_root);

    [[nodiscard]] double c(int n, int m) const noexcept { return c_[index(n, m)]; }
    [[nodiscard]] double s(int n, int m) const noexcept { return s_[index(n, m)]; }

    [[nodiscard]] int max_degree() const noexcept { return max_degree_; }
    /// The highest order with a non-zero coefficient ANYWHERE in the file, which
    /// is not the same as the highest order present in it (GRAV-R-013).
    [[nodiscard]] int max_non_zero_order() const noexcept { return max_non_zero_order_; }
    [[nodiscard]] int max_non_zero_order_at(int n) const noexcept { return max_order_at_[static_cast<std::size_t>(n)]; }
    [[nodiscard]] std::size_t record_count() const noexcept { return records_; }
    [[nodiscard]] TideSystem tide_system() const noexcept { return tide_system_; }
    [[nodiscard]] const std::string& source_path() const noexcept { return path_; }

    /// sigma_n = sqrt(sum_m (C^2 + S^2)), the degree amplitude that GRAV-R-041's
    /// identity is written in terms of.
    [[nodiscard]] double degree_amplitude(int n) const noexcept;

    [[nodiscard]] static constexpr std::size_t index(int n, int m) noexcept {
        return static_cast<std::size_t>(n) * (static_cast<std::size_t>(n) + 1) / 2
             + static_cast<std::size_t>(m);
    }

private:
    // Only load_egm2008 constructs one: there is no way to assemble a
    // CoefficientSet from numbers a caller happens to have.
    CoefficientSet() = default;

    std::vector<double> c_;
    std::vector<double> s_;
    std::vector<int>    max_order_at_;
    std::size_t records_ = 0;
    int max_degree_ = 0;
    int max_non_zero_order_ = 0;
    TideSystem tide_system_ = TideSystem::TideFree;
    std::string path_;
};

}  // namespace odl::gravity
