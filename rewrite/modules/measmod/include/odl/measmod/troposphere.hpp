#pragma once
// odl/measmod/troposphere.hpp — the optical (SLR) tropospheric delay.
//
// SPEC-measmod.md §4.3 (MEAS-R-020 … -R-023, -R-037), §5.5, §7 (MEAS-F-010, -F-022), §8.3.
//
// The zenith delay of Mendes & Pavlis (2004) as TN36 §9.1.1 prints it (equations 9.3–9.7), the mapping function
// FCULa (TN36 §9.1.2, equations 9.8–9.10, Table 9.1) and FCULb (Mendes et al. 2002, Table 1, equation 6), and the
// water-vapour pressure from relative humidity (Marini & Murray 1973, equation 22), which TN36 does not contain.
// Everything is in metres, hectopascals, degrees Celsius and radians; the SLR model passes the station's geodetic
// latitude and ellipsoidal height (TN36 §9.1.1: "the geodetic height of the station").

#include <odl/core/result.hpp>

namespace odl::measmod {

using MeasError = odl::Diagnostic;

/// The surface weather at the station: pressure, temperature, and relative humidity as a fraction (0–1).
struct Atmosphere {
    double pressure_hpa = 0.0;
    double temperature_c = 0.0;
    double relative_humidity = 0.0;
};

/// MEAS-F-022: pressure or wavelength not positive (or not finite), a temperature outside −100…60 °C, a relative
/// humidity outside 0–1. Nothing is clamped.
[[nodiscard]] odl::Result<void, MeasError> validate_atmosphere(const Atmosphere& atmosphere, double wavelength_um);

/// MEAS-R-022: the water-vapour pressure e = Rh · 6.11 · 10^(7.5 t / (237.3 + t)) hPa (Marini & Murray 1973 eq. 22),
/// with `relative_humidity` the fraction Rh/100 and t in °C. Valid where the formula is: it is not defended outside
/// the weather a station reports (the caller validates the atmosphere first).
[[nodiscard]] double water_vapour_pressure_hpa(double relative_humidity, double temperature_c) noexcept;

struct ZenithDelay {
    double total_m = 0.0;
    double hydrostatic_m = 0.0;
    double wet_m = 0.0;
};

/// MEAS-R-020, TN36 §9.1.1 equations (9.3)–(9.7) with the conventional CO₂ content of 375 ppm. `latitude_rad` and
/// `height_m` are the station's geodetic latitude and height, `water_vapour_hpa` the partial pressure of water vapour.
/// MEAS-F-022 for a non-positive pressure or wavelength, a negative vapour pressure or a non-finite input.
[[nodiscard]] odl::Result<ZenithDelay, MeasError> zenith_delay(double latitude_rad, double height_m, double pressure_hpa,
                                                               double water_vapour_hpa, double wavelength_um);

/// A mapping function value and its derivative with respect to sin e (for the partials, MEAS-R-060).
struct Mapping {
    double value = 0.0;
    double d_dsin_e = 0.0;
};

/// MEAS-R-021, FCULa: m(e) = (1 + a₁/(1 + a₂/(1 + a₃))) / (sin e + a₁/(sin e + a₂/(sin e + a₃))),
/// aᵢ = aᵢ₀ + aᵢ₁ tₛ + aᵢ₂ cos φ + aᵢ₃ H (tₛ in °C, H in metres). No validity check: `TroposphereModel::leg` has it.
[[nodiscard]] Mapping mapping_fcula(double sin_e, double latitude_rad, double height_m, double temperature_c) noexcept;

/// MEAS-R-021, FCULb: aᵢ = aᵢ₀ + (aᵢ₁ + aᵢ₂ φ_d²) cos(2π/365.25 · (doy − 28)) + aᵢ₃ H + aᵢ₄ cos φ, φ_d in degrees,
/// `day_of_year` the decimal day of the year. Implemented because it is a published case and for a station with no
/// temperature; the range model uses FCULa.
[[nodiscard]] double mapping_fculb(double sin_e, double latitude_rad, double height_m, double day_of_year) noexcept;

/// The delay of one leg: ZTD · m(sin e), its derivative with respect to sin e, and the two factors.
struct LegDelay {
    double delay_m = 0.0;
    double d_delay_d_sin_e = 0.0;
    double ztd_m = 0.0;
    double mapping = 0.0;
};

/// The troposphere at one station for one observation: the zenith delay is computed once and each leg's mapping at its own
/// elevation (MEAS-R-023, -R-033).
class TroposphereModel {
public:
    /// MEAS-F-022 for a non-physical atmosphere or wavelength. The vapour pressure comes from the humidity (MEAS-R-022);
    /// `latitude_rad` and `height_m` are the system reference point's (MEAS-R-037).
    [[nodiscard]] static odl::Result<TroposphereModel, MeasError> make(double latitude_rad, double height_m,
                                                                       const Atmosphere& atmosphere, double wavelength_um);

    /// MEAS-R-023. `sin_e` is the sine of the elevation of the leg's geometric direction above the local horizontal
    /// (û·ĝ). MEAS-F-010 below 3° — FCULa's stated validity (TN36 §9.1.2: "for elevation angles greater than 3 degrees")
    /// — and for a target on or below the horizon: the mapping function is not extrapolated.
    [[nodiscard]] odl::Result<LegDelay, MeasError> leg(double sin_e) const;

    [[nodiscard]] double zenith_total_m() const noexcept { return ztd_.total_m; }
    [[nodiscard]] const ZenithDelay& zenith() const noexcept { return ztd_; }
    [[nodiscard]] double water_vapour_hpa() const noexcept { return water_vapour_hpa_; }

private:
    TroposphereModel(double latitude_rad, double height_m, double temperature_c, ZenithDelay ztd, double e) noexcept
        : latitude_rad_(latitude_rad), height_m_(height_m), temperature_c_(temperature_c), ztd_(ztd), water_vapour_hpa_(e) {}
    double latitude_rad_, height_m_, temperature_c_;
    ZenithDelay ztd_;
    double water_vapour_hpa_;
};

}  // namespace odl::measmod
