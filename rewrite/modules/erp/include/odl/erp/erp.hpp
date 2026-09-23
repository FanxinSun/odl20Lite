#pragma once
// odl/erp/erp.hpp — Earth albedo and infrared radiation pressure, as a
// dyn::Force plugin over the same photon-pressure kernel SRP uses.
//
// SPEC-photon-pressure PHPR-R-008..R-011.

#include <odl/attitude/attitude.hpp>
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/dynamics/force.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/srp_analytic/srp_analytic.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

#include <functional>
#include <vector>

namespace odl::erp {

using ErpError = odl::Diagnostic;

/// PHPR-R-009: albedo and emissivity as FUNCTIONS of position (geocentric
/// latitude, longitude, radians) and time, not a constant baked into the cap
/// integral -- Knocke's own zonal/seasonal model, or a later gridded
/// product, plugs in by supplying a different function, without the cap
/// integral itself changing (`PHPR-Q-001` carries what that function's own
/// coefficients should be; this type is what it plugs into).
using SurfaceProperty = std::function<double(double lat_rad, double lon_rad,
                                             const odl::time::Epoch& t)>;

/// PHPR-P-4/RS09 Eq. 2.25-2.26, both accessible sources (`RS09`, the
/// independent DORIS/IDS-workshop source) stating the same two numbers.
[[nodiscard]] double constant_albedo_0_3(double lat_rad, double lon_rad,
                                         const odl::time::Epoch& t);
[[nodiscard]] double constant_emissivity_0_7(double lat_rad, double lon_rad,
                                             const odl::time::Epoch& t);

/// PHPR-R-008/R-009 (SPEC-photon-pressure §4.4): the visible-cap integral,
/// in NADIR-CENTRED coordinates (`erp.cpp`'s own header comment has the
/// full derivation and why, re-derived after the manager found the
/// previous global-grid version's own convergence ratio unusable).
/// `n_chi`/`n_az` set the (Delta-chi x Delta-az) grid `PHPR-P-1`'s own
/// convergence-ratio requirement is stated against -- a parameter, not a
/// constant, exactly so a caller (or a test) can refine it and measure the
/// ratio rather than trust a single resolution. `n_chi` spans exactly the
/// visible cap (colatitude from the sub-satellite point, 0 to `PHPR-A-010`'s
/// own beta), not the whole sphere -- there is no longer a wasted majority
/// of cells outside it.
///
/// `r_sat_gcrs_m`: the spacecraft's GCRS position, and it must be strictly
/// above the spherical Earth's own radius (`PHPR-F-005`): beta =
/// arccos(R_E/|r_sat_gcrs_m|) is undefined otherwise. `sun_direction_body`,
/// `v_rel_source_body_m_per_s`: already resolved into body frame by the
/// caller (the same nominal attitude both plugins share) -- this function
/// does not compute attitude itself, since it is identical across every
/// cell and the caller (Erp::accel_only) computes it once; the cap's own
/// integration frame is read directly off `m_gcrs_to_body`'s rows for the
/// same reason (`erp.cpp`'s own header comment).
///
/// Returns the summed force, in NEWTONS, in BODY FRAME -- every cell's own
/// contribution is already in that frame (the kernel's own output), so no
/// per-cell rotation is needed, only a per-cell direction and irradiance.
[[nodiscard]] odl::Result<Vec3, ErpError>
cap_integral(const macromodel::Macromodel& model, const Vec3& r_sat_gcrs_m,
            const macromodel::BodyDirection& sun_direction_body,
            const Vec3& v_rel_source_body_m_per_s, const Mat3& m_gcrs_to_body,
            const odl::time::Epoch& t, const SurfaceProperty& albedo,
            const SurfaceProperty& emissivity, int n_chi, int n_az);

/// `dyn::Force`'s frozen surface, `Srp`'s own shape. Needs the Sun's own
/// direction too, despite the irradiance SOURCE being the Earth: a
/// sun-tracking panel's own normal still follows the Sun regardless of which
/// body's radiation is being computed (`PHPR-R-002`), and the albedo term's
/// own cos(gamma) is the angle between each cell's normal and the Sun.
class Erp final : public dyn::Force {
public:
    Erp(macromodel::Macromodel model, const eph::Ephemeris& ephemeris, odl::time::LeapTable leaps,
       SurfaceProperty albedo, SurfaceProperty emissivity, int n_chi, int n_az);

    [[nodiscard]] dyn::ForceId id() const override;
    [[nodiscard]] const std::vector<dyn::ParameterId>& consumes() const override;
    [[nodiscard]] odl::Result<dyn::ForceEvaluation, dyn::DynError>
    accel(const odl::time::Epoch& t, const frames::Position<frames::Frame::GCRS>& r_m,
         const Vec3& v_m_per_s, const dyn::ParameterSet& params,
         const dyn::ParameterRegistry& registry) const override;

private:
    [[nodiscard]] odl::Result<Vec3, dyn::DynError>
    accel_only(const odl::time::Epoch& t, const Vec3& r_gcrs_m, const Vec3& v_gcrs_m_per_s) const;

    macromodel::Macromodel model_;
    const eph::Ephemeris& ephemeris_;
    odl::time::LeapTable leaps_;
    SurfaceProperty albedo_;
    SurfaceProperty emissivity_;
    int n_chi_, n_az_;
    std::vector<dyn::ParameterId> consumes_;   // always empty, PHPR-R-005's reasoning applies here too
};

}  // namespace odl::erp
