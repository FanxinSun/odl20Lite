// SPEC-measmod.md §4.4 — the Shapiro term.

#include <odl/measmod/shapiro.hpp>

#include <cmath>
#include <sstream>

namespace odl::measmod {

odl::Result<ShapiroLeg, MeasError> shapiro_leg(double r1_m, double r2_m, double rho_m) {
    const double s = r1_m + r2_m;
    if (!std::isfinite(r1_m) || !std::isfinite(r2_m) || !std::isfinite(rho_m) || !(r1_m > 0.0) || !(r2_m > 0.0) || rho_m < 0.0 || !(s > rho_m)) {
        std::ostringstream m;
        m << "the Shapiro term of a leg with r1 = " << r1_m << " m, r2 = " << r2_m << " m, rho = " << rho_m
          << " m has no finite value (it needs r1, r2 > 0 and rho < r1 + r2): a leg through the Earth's centre is a target below the horizon";
        return odl::err("MEAS-F-010", m.str());
    }
    const double k = 2.0 * kGmEarthTt_m3_s2 / (kSpeedOfLight_m_s * kSpeedOfLight_m_s);   // 2 GM⊕ / c², metres
    const double denom = s * s - rho_m * rho_m;
    ShapiroLeg out;
    out.delay_m = k * std::log1p(2.0 * rho_m / (s - rho_m));   // ln[(s + rho)/(s - rho)], well conditioned for a short leg
    out.d_rho = k * 2.0 * s / denom;
    out.d_r1 = -k * 2.0 * rho_m / denom;
    out.d_r2 = out.d_r1;
    return out;
}

}  // namespace odl::measmod
