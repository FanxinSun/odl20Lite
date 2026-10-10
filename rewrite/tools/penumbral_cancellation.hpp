#pragma once
// tools/penumbral_cancellation.hpp — the tool behind SHDW-R-031 and SHDW-R-033 (SPEC-shadow §3.4, §8 Coverage; plan L0 step 8, group C10).
//
// `run` is the whole tool: `penumbral_cancellation [-h]`.  It takes a few minutes and prints the study: Fs of a limb-darkened Sun against a uniform one along ONE stated eclipse passage, found by an actual
// two-body Kepler propagator, for the baseline, a tilt of the orbital plane toward the eclipse cutoff, and eccentric orbits at and off an apse, with the resolution checks and the SUMMARY that SPEC-shadow
// and PROVENANCE quote.  NOT a gate (SHDW-Q-005): a number rightly excused from the gate is not excused from being reproducible (plan §4 rule 3).  Exit 0 done; 2 an argument error; 70 an error the tool
// did not anticipate.

#include <odl/devkit/tool.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <iosfwd>
#include <span>
#include <string>
#include <vector>

namespace odl::tools::penumbral_cancellation {

inline constexpr char kTool[] = "penumbral_cancellation";

// SHDW's own constants — kept numerically identical to conical.hpp / perspective.hpp rather than re-derived, since this tool exists to measure the MODULE's behaviour.
inline constexpr double kAuM = 1.495978707e11;
inline constexpr double kGmEarth = 3.986004415e14;   // l2_floors.cpp's own value, EGM2008-consistent
inline constexpr double kREarthM = 6378137.0;         // conical.hpp's kEarthRadiusM
inline constexpr double kRSunM = 6.957e8;             // conical.hpp's kSunRadiusM
inline constexpr double kEddingtonU = 0.6;            // SPEC-shadow SHDW-R-032: I(mu)/I(1) = 1 - u(1-mu), u = 3/5 exactly

using Vec3 = std::array<double, 3>;

/// The Sun's centre in the frame of the study: on the +x axis at one astronomical unit; the Earth's centre is the origin.
inline constexpr Vec3 kSunPosition = {kAuM, 0.0, 0.0};

// ------------------------------------------------------------------------------------------------------------------------------------------ NumPy's arithmetic, where the printed figures depend on its rounding

/// numpy's pairwise summation of n doubles (loops_utils.h.src, PW_BLOCKSIZE 128, eight accumulators): what np.add.reduce does to a contiguous float64 array.
[[nodiscard]] double pairwise_sum(std::span<const double> a);
/// np.sum / np.mean of a 1-D float64 array: the reduction starts from the identity 0.0 and adds the pairwise sum; the mean divides by the count.
[[nodiscard]] double np_sum(std::span<const double> a);
[[nodiscard]] double np_mean(std::span<const double> a);
/// np.trapezoid(y, x): (diff(x) * (y[1:] + y[:-1]) / 2.0).sum().
[[nodiscard]] double np_trapezoid(std::span<const double> y, std::span<const double> x);
/// np.cumsum: the running sum, in order.
[[nodiscard]] std::vector<double> np_cumsum(std::span<const double> a);
/// np.cross of two 3-vectors and np.linalg.norm / the dot product of BLAS for n = 3 (a sequential sum of the products).
[[nodiscard]] Vec3 cross(const Vec3& a, const Vec3& b);
[[nodiscard]] double dot3(const Vec3& a, const Vec3& b);
[[nodiscard]] double norm3(const Vec3& a);

// ------------------------------------------------------------------------------------------------------------------------------------------ the comparator

/// The Sun's radial brightness, as the cumulative weight out to angle `a` from its own disc centre.  u = 0 is the uniform disc both shadow models assume; u = 0.6 is Eddington's bolometric law.
struct Sky {
    Sky(double a_s_, double u_);
    double a_s;
    double u;
    double S;   // sin(a_s)
    double k;   // cos(a_s)
    [[nodiscard]] double F(double c) const;
    [[nodiscard]] double W(double a) const;
};

/// Fs for a satellite at `sat`, the Sun's centre at `sun` (metres, a spherical occulter of radius `re`): the fraction of the Sun's disc NOT blocked, weighted by brightness profile `u`.  Exact in the radial
/// direction (each blocked interval's ends found by bisection to double precision); the azimuthal sweep is discretised in `nphi` directions.
[[nodiscard]] double fs(const Vec3& sun, const Vec3& sat, double re, double u, int nphi = 900);

// ------------------------------------------------------------------------------------------------------------------------------------------ two-body Kepler propagation

/// Deliberately NOT the constant-angular-rate shortcut a circular orbit would allow: it always solves Kepler's equation (Newton, 80 steps), even at e = 0.
struct Orbit {
    Orbit(double a_, double e_, const Vec3& peri_hat_, const Vec3& q_hat_);
    double a;
    double e;
    Vec3 peri_hat;   // periapsis direction (unit)
    Vec3 q_hat;      // in-plane, perpendicular to peri_hat (unit)
    double n_mean;
    [[nodiscard]] Vec3 pos(double t) const;
    /// The orbital period 2 pi / n.
    [[nodiscard]] double period() const;
};

struct Result {
    bool ok = false;
    double net = 0.0;
    double absint = 0.0;
    double worst_running = 0.0;
    double duration = 0.0;
    double cancel_pct = 0.0;
    double swing = 0.0;
    std::string note;
};

struct StudyOptions {
    int nphi_coarse = 150;
    int nphi_fine = 450;
    int n_samples = 401;
    int coarse_n = 300;
};

/// The pieces of one traversal (study() is their composition; the cut changes no arithmetic).
///   orbit_for                the orbit of (a, e, beta, omega): beta the angle between the orbital plane and the Sun direction, omega the argument of periapsis measured from the eclipse-closest direction
///   angle_from_antisolar     the angle (radians) between the satellite's direction at time t and the antisolar direction (-x)
///   locate_closest_approach  the time of the closest approach to the antisolar direction: a scan of 4000 samples over one period centred on t = 0, then an 80-step ternary search
///   find_window              the window [dt_lo, dt_hi] around t0, in seconds: from 15 % of the penumbra's length before the umbra's edge to 15 % after full sunlight; not ok with a note when there is no eclipse
///                            at the closest approach or the half period never reaches full sunlight
///   sample_times             dt_lo + (dt_hi - dt_lo) i / (n - 1), i = 0 ... n - 1
///   summarize                the net and absolute time integrals of the sampled difference du (trapezoid, as np.trapezoid), the peak of the running integral, the cancellation and the swing
struct Window {
    bool ok = false;
    double dt_lo = 0.0;
    double dt_hi = 0.0;
    std::string note;
};
[[nodiscard]] Orbit orbit_for(double a, double e, double beta, double omega_deg);
[[nodiscard]] double angle_from_antisolar(const Orbit& orb, double t);
[[nodiscard]] double locate_closest_approach(const Orbit& orb);
[[nodiscard]] Window find_window(const Orbit& orb, double t0, const StudyOptions& options);
[[nodiscard]] std::vector<double> sample_times(double dt_lo, double dt_hi, int n_samples);
[[nodiscard]] Result summarize(std::span<const double> du, std::span<const double> tt, double duration);

/// One traversal, fully specified by (a, e, beta, omega): beta the angle between the orbital plane and the Sun direction (0: the shadow axis lies in the orbital plane); omega the argument of periapsis
/// measured from the eclipse-closest direction (0: the eclipse sits exactly at periapsis or apoapsis).  Returns the net and absolute time integrals of (limb - uniform) Fs across ONE side of the
/// penumbral transition -- located by search, not assumed -- plus the peak of the running integral and the cancellation percentage.
[[nodiscard]] Result study(double a, double e, double beta, double omega_deg, const StudyOptions& options = {});

/// "[label]" and the line of figures (or the note), as the study prints each row.
void report(std::ostream& out, const std::string& label, const Result& r);

struct Settings {
    /// If set, called instead of study(): the tests' seam for the printing and the SUMMARY.
    std::function<Result(double a, double e, double beta, double omega_deg, const StudyOptions&)> study;
};
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::penumbral_cancellation
