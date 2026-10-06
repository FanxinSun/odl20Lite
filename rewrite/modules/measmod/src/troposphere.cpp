// SPEC-measmod.md §4.3 — the optical tropospheric delay.

#include <odl/measmod/troposphere.hpp>

#include <cmath>
#include <sstream>

namespace odl::measmod {

namespace {

constexpr double kPi = 3.14159265358979323846;

// MEAS-R-023: FCULa's stated validity, "for elevation angles greater than 3 degrees" (TN36 §9.1.2): sin 3°.
constexpr double kSinMinElevation = 0.052335956242943834;
// NOT-A-UNIT-CROSSING: the tolerance on that bound, a few ulp of a unit-sized quantity, so that an elevation of exactly 3°
// is accepted whichever way it was rounded on its way here
constexpr double kBoundSlack = 4e-16;

}  // namespace

odl::Result<void, MeasError> validate_atmosphere(const Atmosphere& a, double wavelength_um) {
    auto bad = [&](const char* what) -> odl::Result<void, MeasError> {
        std::ostringstream m;
        m << "non-physical atmosphere: " << what << " (pressure " << a.pressure_hpa << " hPa, temperature " << a.temperature_c
          << " °C, relative humidity " << a.relative_humidity << ", wavelength " << wavelength_um
          << " µm); accepted: pressure > 0, temperature −100…60 °C, humidity 0…1, wavelength > 0 — nothing is clamped";
        return odl::err("MEAS-F-022", m.str());
    };
    if (!std::isfinite(a.pressure_hpa) || !(a.pressure_hpa > 0.0)) return bad("the pressure is not positive");
    if (!std::isfinite(a.temperature_c) || a.temperature_c < -100.0 || a.temperature_c > 60.0) return bad("the temperature is outside −100…60 °C");
    if (!std::isfinite(a.relative_humidity) || a.relative_humidity < 0.0 || a.relative_humidity > 1.0) return bad("the relative humidity is outside 0…1");
    if (!std::isfinite(wavelength_um) || !(wavelength_um > 0.0)) return bad("the wavelength is not positive");
    return {};
}

double water_vapour_pressure_hpa(double relative_humidity, double temperature_c) noexcept {
    // Marini & Murray (1973), eq. (22): e = (Rh/100) · 6.11 · 10^(7.5 t / (237.3 + t)) mbar, t in °C
    return relative_humidity * 6.11 * std::pow(10.0, 7.5 * temperature_c / (237.3 + temperature_c));
}

odl::Result<ZenithDelay, MeasError> zenith_delay(double latitude_rad, double height_m, double pressure_hpa,
                                                 double water_vapour_hpa, double wavelength_um) {
    if (!std::isfinite(latitude_rad) || !std::isfinite(height_m) || !std::isfinite(pressure_hpa) || !(pressure_hpa > 0.0) ||
        !std::isfinite(water_vapour_hpa) || water_vapour_hpa < 0.0 || !std::isfinite(wavelength_um) || !(wavelength_um > 0.0)) {
        std::ostringstream m;
        m << "non-physical zenith-delay input: latitude " << latitude_rad << " rad, height " << height_m << " m, pressure " << pressure_hpa
          << " hPa, water-vapour pressure " << water_vapour_hpa << " hPa, wavelength " << wavelength_um << " µm";
        return odl::err("MEAS-F-022", m.str());
    }
    // TN36 §9.1.1, equations (9.3)–(9.7)
    const double fs = 1.0 - 0.00266 * std::cos(2.0 * latitude_rad) - 0.00000028 * height_m;            // (9.4)
    const double k0 = 238.0185, k2 = 57.362, k1s = 19990.975, k3s = 579.55174;                         // µm⁻²
    const double sigma2 = (1.0 / wavelength_um) * (1.0 / wavelength_um);
    const double c_co2 = 1.0 + 0.534e-6 * (375.0 - 450.0);                                             // 375 ppm, the conventional content
    const double fh = 1e-2 * (k1s * (k0 + sigma2) / ((k0 - sigma2) * (k0 - sigma2)) +
                              k3s * (k2 + sigma2) / ((k2 - sigma2) * (k2 - sigma2))) * c_co2;          // (9.5)
    const double w0 = 295.235, w1 = 2.6422, w2 = -0.032380, w3 = 0.004028;
    const double fnh = 0.003101 * (w0 + 3.0 * w1 * sigma2 + 5.0 * w2 * sigma2 * sigma2 + 7.0 * w3 * sigma2 * sigma2 * sigma2);   // (9.7)
    ZenithDelay z;
    z.hydrostatic_m = 0.002416579 * fh / fs * pressure_hpa;                                            // (9.3)
    z.wet_m = 1e-4 * (5.316 * fnh - 3.759 * fh) * water_vapour_hpa / fs;                               // (9.6)
    z.total_m = z.hydrostatic_m + z.wet_m;
    return z;
}

Mapping mapping_fcula(double s, double latitude_rad, double height_m, double temperature_c) noexcept {
    // TN36 Table 9.1: aij; ai1 in °C⁻¹ and ai3 in m⁻¹ (coefficients exactly as printed)
    const double cphi = std::cos(latitude_rad);
    const double a1 = 12100.8e-7 + 1729.5e-9 * temperature_c + 319.1e-7 * cphi - 1847.8e-11 * height_m;
    const double a2 = 30496.5e-7 + 234.6e-8 * temperature_c - 103.5e-6 * cphi - 185.6e-10 * height_m;
    const double a3 = 6877.7e-5 + 197.2e-7 * temperature_c - 345.8e-5 * cphi + 106.0e-9 * height_m;
    // (9.9): m = N / D, N = 1 + a1/(1 + a2/(1 + a3)), D = s + a1/(s + a2/(s + a3))
    const double n = 1.0 + a1 / (1.0 + a2 / (1.0 + a3));
    const double q = s + a3;
    const double r = s + a2 / q;
    const double d = s + a1 / r;
    const double dd = 1.0 - (a1 / (r * r)) * (1.0 - a2 / (q * q));
    return Mapping{n / d, -n * dd / (d * d)};
}

double mapping_fculb(double s, double latitude_rad, double height_m, double day_of_year) noexcept {
    // Mendes et al. (2002) Table 1 and equation (6), signs read from the rendered page (the text layer drops them) and verified by
    // the IERS FCUL_B printed case: aᵢ = aᵢ₀ + (aᵢ₁ + aᵢ₂ φ_d²) cos(2π/365.25 (doy − 28)) + aᵢ₃ H + aᵢ₄ cos φ
    const double phi_d = latitude_rad * 180.0 / kPi;
    const double season = std::cos(2.0 * kPi / 365.25 * (day_of_year - 28.0));
    const double cphi = std::cos(latitude_rad);
    const double a1 = 11613.1e-7 + (-933.8e-8 + -595.8e-11 * phi_d * phi_d) * season - 2462.7e-11 * height_m + 1286.4e-7 * cphi;
    const double a2 = 29815.1e-7 + (-56.9e-7 + -165.5e-10 * phi_d * phi_d) * season - 272.5e-10 * height_m + 302.0e-7 * cphi;
    const double a3 = 68183.9e-6 + (93.5e-6 + -239.4e-9 * phi_d * phi_d) * season + 30.4e-9 * height_m - 230.8e-5 * cphi;
    return (1.0 + a1 / (1.0 + a2 / (1.0 + a3))) / (s + a1 / (s + a2 / (s + a3)));
}

odl::Result<TroposphereModel, MeasError> TroposphereModel::make(double latitude_rad, double height_m, const Atmosphere& atmosphere,
                                                                double wavelength_um) {
    auto ok = validate_atmosphere(atmosphere, wavelength_um);
    if (!ok) return odl::err(ok.error());
    const double e = water_vapour_pressure_hpa(atmosphere.relative_humidity, atmosphere.temperature_c);
    auto z = zenith_delay(latitude_rad, height_m, atmosphere.pressure_hpa, e, wavelength_um);
    if (!z) return odl::err(z.error());
    return TroposphereModel(latitude_rad, height_m, atmosphere.temperature_c, *z, e);
}

odl::Result<LegDelay, MeasError> TroposphereModel::leg(double sin_e) const {
    if (!std::isfinite(sin_e) || sin_e < kSinMinElevation - kBoundSlack) {
        std::ostringstream m;
        m << "the elevation of the leg (sin e = " << sin_e << ", " << (std::isfinite(sin_e) && sin_e >= -1.0 && sin_e <= 1.0 ? std::asin(sin_e) * 180.0 / kPi : 0.0)
          << "°) is below 3°, or the target is below the horizon: the mapping function FCULa is valid \"for elevation angles greater than 3 degrees\" "
             "(TN36 §9.1.2) and is not extrapolated";
        return odl::err("MEAS-F-010", m.str());
    }
    const Mapping m = mapping_fcula(sin_e, latitude_rad_, height_m_, temperature_c_);
    LegDelay out;
    out.ztd_m = ztd_.total_m;
    out.mapping = m.value;
    out.delay_m = ztd_.total_m * m.value;
    out.d_delay_d_sin_e = ztd_.total_m * m.d_dsin_e;
    return out;
}

}  // namespace odl::measmod
