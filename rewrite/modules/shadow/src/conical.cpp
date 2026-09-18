// conical.cpp — SPEC-shadow.md.

#include <odl/shadow/conical.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace odl::shadow {
namespace {

/// The area of the lens where two circles of radii a and b overlap, their
/// centres d apart. Closed form -- two circular segments -- so SHDW-P-2 has no
/// quadrature and therefore no quadrature tolerance.
double lens_area(double a, double b, double d) noexcept {
    if (d >= a + b) return 0.0;                       // disjoint
    if (d <= std::abs(a - b)) {                       // one wholly inside the other
        const double r = std::min(a, b);
        return std::acos(-1.0) * r * r;
    }
    const double d2 = d * d, a2 = a * a, b2 = b * b;
    const double alpha = std::acos(std::clamp((d2 + a2 - b2) / (2.0 * d * a), -1.0, 1.0));
    const double beta  = std::acos(std::clamp((d2 + b2 - a2) / (2.0 * d * b), -1.0, 1.0));
    return a2 * (alpha - std::sin(2.0 * alpha) / 2.0)
         + b2 * (beta  - std::sin(2.0 * beta)  / 2.0);
}

}  // namespace

double umbra_apex_distance_m() noexcept {
    constexpr double kAu = 1.495978707e11;            // m
    return kEarthRadiusM * kAu / (kSunRadiusM - kEarthRadiusM);
}

odl::Result<ShadowResult, ShadowError>
conical(const frames::Position<frames::Frame::GCRS>& sun,
        const frames::Position<frames::Frame::GCRS>& sat) {
    const Vec3 rs = sun.metres(), r = sat.metres();
    const double rn = r.norm(), sn = rs.norm();
    if (!(rn > 0.0)) {
        return odl::err(ShadowError{"SHDW-F-001", "the satellite position is the origin"});
    }
    if (!(sn > 0.0)) {
        return odl::err(ShadowError{"SHDW-F-001", "the Sun position is the origin"});
    }
    if (rn < kEarthRadiusM) {
        return odl::err(ShadowError{"SHDW-F-002",
            "the satellite is inside the occulting body: |r| = " + std::to_string(rn) +
            " m against a radius of " + std::to_string(kEarthRadiusM) + " m."});
    }

    // The satellite-to-Sun vector, and the apparent angular radii as seen FROM
    // THE SATELLITE. The Sun is a DISC, which is the choice SHDW-A-004 tests: a
    // point source would give a penumbra of zero angular width.
    const Vec3 to_sun{rs.x - r.x, rs.y - r.y, rs.z - r.z};
    const double ds = to_sun.norm();
    const double a_sun   = std::asin(std::clamp(kSunRadiusM / ds, -1.0, 1.0));
    const double a_earth = std::asin(std::clamp(kEarthRadiusM / rn, -1.0, 1.0));
    // the angle between the directions to the Sun's centre and to the Earth's
    const double cos_sep = -(r.x * to_sun.x + r.y * to_sun.y + r.z * to_sun.z) / (rn * ds);
    const double sep = std::acos(std::clamp(cos_sep, -1.0, 1.0));

    ShadowResult out;
    if (sep >= a_sun + a_earth) {                     // the discs are disjoint
        out.fraction = 1.0; out.state = State::sunlight;
        return out;
    }
    if (sep <= a_earth - a_sun) {                     // the Sun is wholly hidden
        out.fraction = 0.0; out.state = State::umbra;
        return out;
    }
    if (sep <= a_sun - a_earth) {
        // THE ANNULAR BRANCH. The Earth's disc lies wholly inside the Sun's, so a
        // ring of Sun remains. It needs the satellite BEYOND the umbra apex at
        // 1.3842e6 km, which no orbit in this plan reaches -- see the header and
        // SHDW-A-005, whose name records that its test is synthetic.
        out.fraction = 1.0 - (a_earth * a_earth) / (a_sun * a_sun);
        out.state = State::annular;
        return out;
    }
    // PENUMBRA: the true occulted-area ratio, not a linear ramp and not a
    // smoothstep. "the ratio of the unblocked solar disk area to the area of the
    // whole solar disk".
    const double pi = std::acos(-1.0);
    const double solar_area = pi * a_sun * a_sun;
    out.fraction = std::clamp(1.0 - lens_area(a_sun, a_earth, sep) / solar_area, 0.0, 1.0);
    out.state = State::penumbra;
    return out;
}

}  // namespace odl::shadow
