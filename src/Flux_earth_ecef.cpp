/*! @file Flux_earth_ecef.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux_earth_ecef class.
 */

#include "../include/Flux_earth_ecef.h"

Flux_earth_ecef::Flux_earth_ecef()
{
    // Average long and short wavelength fluxes from CERES data quality summary
    avg_earth_flux_lw = 239.6;
    avg_earth_flux_sw = 99.6;

    // Calculate the average "radius" of the CERES TOA ellipsoid
    constexpr double a_ceres = 6408.1370;
    constexpr double b_ceres = 6386.6517;
    constexpr double ceres_earth_rad = 0.5 * (a_ceres + b_ceres);
    ceres_earth_rad2 = ceres_earth_rad * ceres_earth_rad;
}

void Flux_earth_ecef::add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num)
{
    eci_flux_num += 0;
    ecef_flux_num += 1;
}

void Flux_earth_ecef::compute_flux()
{
    double scale_factor = ceres_earth_rad2 / state->r2;

    Fluxstruct ecef_flux;

    ecef_flux.mag_lw = scale_factor * avg_earth_flux_lw;
    ecef_flux.mag_sw = scale_factor * avg_earth_flux_sw;

    // Apply additional scaling to the shortwave flux based on sun position:
    ecef_flux.mag_sw *=
        0.5 * dot_product(state->ecef_sun_hat, state->ecef_rso_hat) + 0.5;

    ecef_flux.dir = state->ecef_rso_hat;

    state->ecef_fluxes.push_back(ecef_flux);
}
