#pragma once
// odl/ephemerides/ephemeris.hpp — where a solar-system body is.
//
// SPEC-ephemerides.md. Two things about this interface are unlike the rest of
// the tree, and both are ruled rather than assumed:
//
//   * `open` takes PATHS, not bytes (EPH-Q-002). SPK kernels reach 114 MB and
//     CALCEPH memory-maps them. This is the tree's ONE exception to the
//     loaders-take-bytes convention; every other loader still takes bytes. The
//     hash is verified by the fetcher BEFORE a path is handed over, so the
//     property that convention protected is preserved by another mechanism.
//   * States come back in `Frame::BCRS`, added to SPEC-frames at v1.4. A
//     barycentric vector is not a geocentric one and they differ by 1.5e8 km.

#include <odl/core/result.hpp>
#include <odl/ephemerides/body.hpp>
#include <odl/frames/state.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <memory>
#include <string>
#include <vector>

namespace odl::eph {

using EphError = odl::Diagnostic;

struct EphProvenance {
    std::string source_id;    ///< manifest id
    std::string path;
    std::string sha256;
};

struct Coverage {
    double first_jd_tdb = 0.0;
    double last_jd_tdb = 0.0;
};

class Ephemeris {
public:
    /// Kernel paths, which must come from the manifest cache (EPH-R-040).
    static odl::Result<Ephemeris, EphError> open(const std::vector<std::string>& cache_paths,
                                                  std::vector<EphProvenance> provenance);

    Ephemeris(Ephemeris&&) noexcept;
    Ephemeris& operator=(Ephemeris&&) noexcept;
    ~Ephemeris();
    Ephemeris(const Ephemeris&) = delete;

    /// Per BODY per kernel, not per kernel: one kernel covers different bodies
    /// over different spans, and de440s does (EPH-R-041).
    [[nodiscard]] odl::Result<Coverage, EphError> coverage(Body b) const;

    /// A constant from the kernel's own header — "AU", "EMRAT", "GM*".
    [[nodiscard]] odl::Result<double, EphError> constant(const std::string& name) const;

    /// Position in km and velocity in km s⁻¹, in the BCRS (EPH-R-013).
    [[nodiscard]] odl::Result<odl::frames::State<odl::frames::Frame::BCRS>, EphError>
    state(Body target, Body centre, const odl::time::Epoch& when,
          const odl::time::LeapTable& leaps) const;

    /// **TDB − TT**, in that sense (EPH-R-004). The kernel's own body 16 is
    /// TT−TDB, the opposite, so this negates it — which is exactly the sign the
    /// convention exists to pin down.
    [[nodiscard]] odl::Result<odl::time::Duration, EphError>
    tdb_minus_tt(const odl::time::Epoch& when, const odl::time::LeapTable& leaps) const;

    [[nodiscard]] const std::vector<EphProvenance>& provenance() const noexcept {
        return provenance_;
    }

    /// The IAU 2012 astronomical unit, in km. DE440 adopts it [PARK21 §2].
    ///
    /// Since IAU 2012 Resolution B2 the au is a DEFINING constant — exactly
    /// 149 597 870 700 m — not a measured quantity. That matters here because
    /// **an SPK kernel carries no constants at all** (measured: a .bsp reports a
    /// constant count of zero), so EPH-R-012's "read it from the kernel" is not
    /// satisfiable on the route the plan mandates. Reading a defined constant
    /// from a file was never more authoritative than the definition.
    static constexpr double kAstronomicalUnitKm = 149597870.700;

    struct AstronomicalUnit {
        double km = kAstronomicalUnitKm;
        bool from_kernel = false;   ///< recorded in provenance either way
    };
    /// The kernel's AU where it has one, checked against the defining value; the
    /// defining value where it has none, with `from_kernel` false so a run can
    /// record which it used.
    [[nodiscard]] AstronomicalUnit astronomical_unit() const noexcept;

private:
    Ephemeris() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<EphProvenance> provenance_;
};

}  // namespace odl::eph
