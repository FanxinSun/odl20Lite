#pragma once
// odl/gravity/field.hpp — the geopotential, and the conventional model built
// from it.
//
// TWO TYPES AND NOT ONE, because they are not the same object.  A GravityModel
// is what the publisher distributes: a coefficient set with its own matched
// scaling parameters.  A ConventionalField is TN36-6 §6.1's conventional model,
// which differs from the distributed file in four coefficients and is a function
// of epoch.  GRAV-R-052 keeps the first immutable so that GRAV-A-009 can still
// compare the loaded values against the numbers the Conventions print, after
// the substitution has happened.
//
// GRAV-R-035: the expression 1/cos(phi) does not appear in the implementation.
// The pole is an ordinary point of this module, not a special case, because the
// recursion propagates Pbar_nm / cos^m(phi) and the longitude factors carry the
// powers of cos(phi) back in progressively.  See src/field.cpp.

#include <odl/core/result.hpp>
#include <odl/frames/vector.hpp>
#include <odl/gravity/coefficients.hpp>
#include <odl/gravity/degree.hpp>
#include <odl/gravity/scaling.hpp>
#include <odl/gravity/secular_pole.hpp>
#include <odl/time/epoch.hpp>

#include <memory>
#include <string>
#include <vector>

namespace odl::gravity {

/// One coefficient the conventional model changes relative to the distributed
/// file, with everything needed to audit it (GRAV-R-025).
struct Substitution {
    int n = 0;
    int m = 0;
    char which = 'C';          ///< 'C' or 'S'
    double from = 0.0;
    double to = 0.0;
    std::string authority;     ///< the clause of TN36 that required it
};

/// What a model carries about where it came from.
struct Provenance {
    std::string coefficient_file;
    std::string coefficient_sha256_declared_in;   ///< the manifest entry, not a recomputed hash
    std::size_t records = 0;
    TideSystem file_tide_system = TideSystem::TideFree;
    std::string scaling_source;
    GmCompatibility gm_compatibility = GmCompatibility::TT;
    bool extrapolated_beyond_pole_fit = false;
};

class ConventionalField;

/// The recursion coefficients a_nm and b_nm of SPEC-gravity §4.4, tabulated
/// once.  GRAV-R-032 requires each to be formed from EXACT INTEGER expressions
/// and then converted, never accumulated from its predecessor, so that no error
/// compounds along a column; forming them once from exact integers and reading
/// the table satisfies that and removes two square roots from the inner loop.
/// Every product involved is below 2^53 at degree 2190, so the integer stage is
/// exact: the largest is (2n+1)(n+m-1)(n-m-1) < 4.3 x 10^10.
class RecursionTable {
public:
    explicit RecursionTable(int max_degree);
    [[nodiscard]] double a(int n, int m) const noexcept { return a_[CoefficientSet::index(n, m)]; }
    [[nodiscard]] double b(int n, int m) const noexcept { return b_[CoefficientSet::index(n, m)]; }
    /// Pbar'_mm, the sectorial seed with cos^m(phi) factored out.  Bounded by
    /// 10.277577 over the whole model, which is the measurement that makes the
    /// factored form necessary rather than merely tidy (GRAV-P-7).
    [[nodiscard]] double sectorial(int m) const noexcept { return pmm_[static_cast<std::size_t>(m)]; }

private:
    std::vector<double> a_, b_, pmm_;
};

/// What a loaded model owns.  Held by shared_ptr so that a ConventionalField
/// cannot outlive the coefficients it reads, without either copying 38 MB per
/// epoch or introducing the global state GRAV-R-054 forbids.
struct ModelData {
    CoefficientSet coefficients;
    RecursionTable recursion;
};

class GravityModel {
public:
    GravityModel() = delete;

    [[nodiscard]] static odl::Result<GravityModel, GravityError>
    load(const std::string& coefficient_path, const std::string& cache_root,
         ScalingParameters scaling);

    /// TN36-6 §6.1 applied at an epoch: Table 6.2's three zonals with their
    /// rates (6.4), and the figure-axis terms (6.5) from the secular pole.
    [[nodiscard]] odl::Result<ConventionalField, GravityError>
    conventional(const odl::time::Epoch& tt,
                 bool extrapolate_secular_terms_beyond_fit = false) const;

    [[nodiscard]] const CoefficientSet& coefficients() const noexcept { return d_->coefficients; }
    [[nodiscard]] const ScalingParameters& scaling() const noexcept { return scaling_; }
    [[nodiscard]] const Provenance& provenance() const noexcept { return prov_; }

private:
    GravityModel(std::shared_ptr<const ModelData> d, ScalingParameters s, Provenance p)
        : d_(std::move(d)), scaling_(std::move(s)), prov_(std::move(p)) {}

    std::shared_ptr<const ModelData> d_;
    ScalingParameters scaling_;
    Provenance prov_;
};

class ConventionalField {
public:
    ConventionalField() = delete;

    [[nodiscard]] odl::Result<frames::ItrsAcceleration, GravityError>
    acceleration(const frames::ItrsPosition& at, Degree degree, Order order) const;

    [[nodiscard]] odl::Result<double, GravityError>
    potential(const frames::ItrsPosition& at, Degree degree, Order order) const;

    /// The per-degree acceleration contributions, degree 0 first.  This exists
    /// for GRAV-A-001, which checks the field against the closed-form degree
    /// variance identity of GRAV-R-041 degree by degree; forming it as a
    /// difference of truncated evaluations would cost O(N^2).  It returns a
    /// value rather than filling a caller's buffer, per the §5 convention.
    [[nodiscard]] odl::Result<std::vector<Vec3>, GravityError>
    acceleration_by_degree(const frames::ItrsPosition& at, Degree degree, Order order) const;

    /// GRAV-R-041.  The RMS over a sphere of radius r of the acceleration
    /// discarded by truncating at `degree`, computed from the coefficients
    /// alone and without evaluating the field anywhere.
    [[nodiscard]] odl::Result<double, GravityError>
    truncation_rms(double radius_m, Degree degree) const;

    [[nodiscard]] TideSystem tide_system() const noexcept { return tide_system_; }
    [[nodiscard]] const std::vector<Substitution>& substitutions() const noexcept { return subs_; }
    [[nodiscard]] const Provenance& provenance() const noexcept { return prov_; }
    [[nodiscard]] const ScalingParameters& scaling() const noexcept { return scaling_; }
    [[nodiscard]] const PoleCoordinates& pole() const noexcept { return pole_; }
    [[nodiscard]] const RecursionTable& recursion() const noexcept { return d_->recursion; }
    /// The conventional coefficients: the file's, with GRAV-R-020's and
    /// GRAV-R-022's four substitutions applied.  The distributed values remain
    /// reachable through the model, which is what GRAV-A-009 needs.
    [[nodiscard]] double c(int n, int m) const noexcept {
        // Five coefficients differ from the distributed file and the rest do
        // not, so the test is a compare and not a lookup.  Defined here rather
        // than in the source because it is called once per term of a sum with
        // 2 401 333 terms.
        if (n <= 4) {
            if (n == 2 && m == 0) return c20_;
            if (n == 2 && m == 1) return c21_;
            if (n == 3 && m == 0) return c30_;
            if (n == 4 && m == 0) return c40_;
        }
        return d_->coefficients.c(n, m);
    }
    [[nodiscard]] double s(int n, int m) const noexcept {
        if (n == 2 && m == 1) return s21_;
        return d_->coefficients.s(n, m);
    }
    /// sigma_n of the CONVENTIONAL coefficients, which differ from the file's at
    /// degrees 2, 3 and 4.
    [[nodiscard]] double degree_amplitude(int n) const noexcept;
    [[nodiscard]] int max_degree() const noexcept { return d_->coefficients.max_degree(); }
    [[nodiscard]] const CoefficientSet& distributed() const noexcept { return d_->coefficients; }

private:
    friend class GravityModel;
    ConventionalField(std::shared_ptr<const ModelData> d, ScalingParameters s, Provenance p,
                      PoleCoordinates pole, std::vector<Substitution> subs,
                      double c20, double c21, double s21, double c30, double c40)
        : d_(std::move(d)), scaling_(std::move(s)), prov_(std::move(p)), pole_(pole),
          subs_(std::move(subs)), c20_(c20), c21_(c21), s21_(s21), c30_(c30), c40_(c40) {}

    std::shared_ptr<const ModelData> d_;
    ScalingParameters scaling_;
    Provenance prov_;
    PoleCoordinates pole_;
    std::vector<Substitution> subs_;
    double c20_ = 0.0, c21_ = 0.0, s21_ = 0.0, c30_ = 0.0, c40_ = 0.0;
    // GRAV-R-021: after Table 6.2's substitution the field is zero-tide, and
    // GRAV-R-008 requires it to say so rather than have it documented.
    TideSystem tide_system_ = TideSystem::ZeroTide;
};

}  // namespace odl::gravity
