#pragma once
// odl/harness/gnss_campaign.hpp — L8 step 1: GNSS against IGS precise orbits (plan/subplan_L8/L8-1.md, registered 2026-11-27, rulings L8-R1 and L8-R2).
//
// A fit is the batch estimate of one satellite's 96-epoch IGS final orbit (positions of the SP3 file, ITRS, metres, unit weight) from the GCRS state at the middle epoch (epoch 48, backward and forward chained propagation,
// integrator tolerance 1e-12) and force parameters:
//   cannonball / box-wing: [x, y, z, vx, vy, vz, srp_scale]                                  (srp_scale carried by forcemodel's ScaledForce over Srp)
//   ECOM2 (d4b1_order):    [x, y, z, vx, vy, vz, D0, D2c, D2s, D4c, D4s, Y0, B0, B1c, B1s]    (odl::ecom::Ecom; coefficients in m/s^2, started at zero)
// over Gravity (degree 10, secular pole extrapolated), the Sun and the Moon, the solid / FES2004 (degree 7) / solid-pole tides and the three relativity terms.  The model is the one of the L7 exit gate (tests/l7_exit/),
// made a library.  NO PATH IS EMBEDDED HERE: every file arrives as an argument.
//
// A GnssWorld holds the pinned inputs; the CALCEPH ephemeris handle in it is NOT thread-safe, so a thread owns its own GnssWorld.
#include <odl/core/result.hpp>
#include <odl/core/vec3.hpp>
#include <odl/estimation/levenberg_marquardt.hpp>
#include <odl/harness/statistics.hpp>
#include <odl/time/epoch.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace odl::harness {

struct GnssWorldPaths {
    std::string leap_seconds;      ///< IERS Leap_Second.dat
    std::string de440s;            ///< the JPL SPK
    std::string gm;                ///< the NAIF gravitational-parameter kernel (text)
    std::string egm2008;           ///< EGM2008 coefficients
    std::string fes2004;           ///< FES2004 ocean tide Cnm/Snm
    std::string c04;               ///< IERS EOP C04
    std::string cache_root;        ///< the manifest cache root the loaders check their files against
};

class GnssWorld {
public:
    ~GnssWorld();
    GnssWorld(const GnssWorld&) = delete;
    GnssWorld& operator=(const GnssWorld&) = delete;
    /// Loads every input; a file that does not read is a refusal (HARN-F-020) carrying the loader's own identifier and text.
    [[nodiscard]] static odl::Result<std::unique_ptr<GnssWorld>, HarnessError> load(const GnssWorldPaths& paths);

    struct Impl;
    [[nodiscard]] const Impl& impl() const noexcept { return *impl_; }

private:
    GnssWorld();
    std::unique_ptr<Impl> impl_;
};

/// One satellite's whole arc of an SP3 file.
struct Sp3Arc {
    std::string satellite;
    std::vector<odl::time::Epoch> epochs;     ///< 96, 900 s apart
    std::vector<Vec3> p_itrs_m;               ///< the file's positions, metres
    std::size_t clock_absent_records = 0;     ///< records of this satellite whose clock is the absent marker (position valid)
    odl::time::Calendar first_calendar;       ///< the calendar of the first epoch, as the file prints it
};

/// Reads the whole arc of `satellite` from SP3 text, or refuses with EST-F-901 naming the epoch: a position component >= 999999.0 (POSITIONS ONLY: a record whose clock alone is 999999.999999 is accepted), a record missing,
/// an epoch count other than 96, a spacing other than 900 s, a header flag that is not P, or a reader refusal.
[[nodiscard]] odl::Result<Sp3Arc, HarnessError> read_sp3_arc(const GnssWorld& world, std::string_view sp3_text, const std::string& satellite);

/// The satellites of an SP3 file's header, sorted ("G01" .. "G32").
[[nodiscard]] odl::Result<std::vector<std::string>, HarnessError> list_sp3_prns(std::string_view sp3_text);

/// A cannonball template: the nominal A*C_R/m is area * cr / mass; the fitted srp_scale multiplies it.
struct CannonballTemplate {
    double area_m2 = 1.0;
    double mass_kg = 1.0;
    double cr = 1.0;
    [[nodiscard]] double a_cr_over_m() const noexcept { return area_m2 * cr / mass_kg; }
};

/// L8-R1: the blind, hardware-agnostic template of the block recovery: A = 1 m^2, m = 50 kg, C_R = 1 (A C_R / m = 0.02 m^2/kg).
[[nodiscard]] CannonballTemplate blind_template() noexcept;

/// EST-Q-011's block-referenced template of the baselines: the sun-pointing surfaces' total area of the block's macromodel (Block IIF for G01, IIR-M for G05), its cited mass, C_R = 1.
[[nodiscard]] odl::Result<CannonballTemplate, HarnessError> block_referenced_template(bool block_iif);

struct FitOptions {
    int gravity_degree = 10;
    std::size_t max_iterations = 100;
};

struct GnssFitResult {
    std::string model;                       ///< "cannonball", "box-wing" or "ecom2-d4b1"
    std::string status;                      ///< odl::estimation::to_string(LmStatus)
    bool converged = false;                  ///< converged_gradient, converged_residual or converged_step
    std::string message;
    std::size_t passes = 0, accepted = 0, rejected = 0;
    std::vector<std::string> parameter_names;
    std::vector<double> theta_start, theta_hat;
    double s_n_start_m = 0.0;                ///< S_N at the start
    OrbitResidualStats stats;                ///< at the solution
    double min_shadow_nu = 1.0;              ///< the smallest conical shadow function over the 96 epochs of the fitted trajectory (1 = never in shadow)
    double srp_scale = 0.0;                  ///< cannonball and box-wing: the fitted scale
    double a_cr_over_m = 0.0;                ///< cannonball: srp_scale * the template's A*C_R/m (m^2/kg); 0 otherwise
    [[nodiscard]] double start_over_end() const noexcept { return stats.s_n_m > 0.0 ? s_n_start_m / stats.s_n_m : 0.0; }
};

[[nodiscard]] odl::Result<GnssFitResult, HarnessError> fit_gnss_cannonball(const GnssWorld& world, const Sp3Arc& arc, const CannonballTemplate& tpl, const FitOptions& options = {});
/// `block_iif`: gps_block_iif() (G01 = SVN63), else gps_block_iir_m() (G05 = SVN50).
[[nodiscard]] odl::Result<GnssFitResult, HarnessError> fit_gnss_box_wing(const GnssWorld& world, const Sp3Arc& arc, bool block_iif, const FitOptions& options = {});
[[nodiscard]] odl::Result<GnssFitResult, HarnessError> fit_gnss_ecom2(const GnssWorld& world, const Sp3Arc& arc, const FitOptions& options = {});

}  // namespace odl::harness
