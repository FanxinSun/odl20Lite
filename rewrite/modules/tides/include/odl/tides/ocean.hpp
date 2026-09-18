#pragma once
// odl/tides/ocean.hpp — TN36-6 §6.3, the ocean tide, from FES2004.
//
// THE FILE'S OWN HEADER IS THE AUTHORITY ON WHAT IT CONTAINS, AND IT DOES NOT
// MATCH CHAPTER 6'S DESCRIPTION OF IT (PERT-R-020a).  Its first three lines say:
// unit 1e-11; degree and order (100,100); the LONG PERIOD band is from FES2002
// up to (50,50), not FES2004; the equilibrium Om1/Om2 waves are ALREADY
// INCLUDED; the atmospheric tide is NOT.  §6.3.2 describes how to model Om1 and
// Om2 as equilibrium waves and gives the equation — and applying it would double
// waves the file already carries (PERT-R-025).  So the header is read and
// checked, and a file whose header differs is refused (PERT-F-011).
//
// THE ZONAL TERMS CARRY A CONVENTION THAT IS OTHERWISE WRONG BY A FACTOR OF TWO
// (PERT-R-021).  FES2004 sets the retrograde zonal coefficients to zero and
// DOUBLES the prograde ones, so after (6.15) the dC_n0 have their expected value
// but dS_n0 must be SET TO ZERO.  A reader who applies (6.15) uniformly gets a
// spurious dS_n0 and no warning.

#include <odl/core/result.hpp>
#include <odl/eop/fundamental_arguments.hpp>
#include <odl/tides/increments.hpp>

#include <array>
#include <string>
#include <vector>

namespace odl::tides {

/// What the file says about itself, read rather than assumed.
struct OceanTideHeader {
    std::string model;               ///< "FES2004 normalized model (fev. 2004) up to (100,100)"
    double unit = 0.0;               ///< 1e-11
    int max_degree = 0;
    int max_order = 0;
    bool long_period_from_fes2002 = false;
    int long_period_max_degree = 0;
    bool includes_equilibrium_omega = false;
    bool includes_atmospheric_tide = true;
};

class OceanTide {
public:
    [[nodiscard]] static odl::Result<OceanTide, TidesError>
    load(const std::string& path, const std::string& cache_root);

    [[nodiscard]] odl::Result<TideIncrements, TidesError>
    increments(const eop::tides::Arguments& args, int max_degree, int max_order) const;

    [[nodiscard]] const OceanTideHeader& header() const noexcept { return header_; }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_.size(); }
    [[nodiscard]] std::size_t waves() const noexcept { return wave_count_; }

    /// The RMS over the sphere of the acceleration this model's degrees above
    /// `degree` contribute at radius `r`, by the identity SPEC-gravity
    /// GRAV-R-041 states — the same instrument the static field's truncation
    /// uses, so the two are comparable.
    [[nodiscard]] double truncation_rms(double radius_m, int degree) const noexcept;
    /// The smallest per-degree RMS this model computes and KEEPS at `degree`.
    [[nodiscard]] double smallest_kept_rms(double radius_m, int degree) const noexcept;

    /// PERT-R-022a: the criterion, not a number.  The lowest degree at which the
    /// truncation error falls below the smallest term kept, at this radius.
    [[nodiscard]] int degree_meeting_criterion(double radius_m) const noexcept;

private:
    OceanTide() = default;
    struct Row { double doodson[6]; int n, m; double cp, sp, cm, sm; };
    std::vector<Row> rows_;
    std::vector<double> variance_by_degree_;
    OceanTideHeader header_;
    std::size_t wave_count_ = 0;
    std::string path_;
};

/// The Doodson arguments (tau, s, h, p, N', ps) from the Delaunay ones, so that
/// a table indexed either way uses ONE set of fundamental arguments.  Checked
/// against Table 6.5a, which prints both.
[[nodiscard]] std::array<double, 6> doodson_arguments(const eop::tides::Arguments& a) noexcept;

}  // namespace odl::tides
