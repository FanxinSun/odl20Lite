/*! @file Flux_earth_eci.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux_earth_eci class.
 */

#include "../include/Flux_earth_eci.h"

Flux_earth_eci::Flux_earth_eci()
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

void Flux_earth_eci::add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num)
{
    eci_flux_num += 1;
    ecef_flux_num += 0;
}

void Flux_earth_eci::compute_flux()
{
    double scale_factor = ceres_earth_rad2 / state->r2;

    Fluxstruct eci_flux;

    eci_flux.mag_lw = scale_factor * avg_earth_flux_lw;
    eci_flux.mag_sw = scale_factor * avg_earth_flux_sw;

    // Apply additional scaling to the shortwave flux based on sun position:
    eci_flux.mag_sw *=
        0.5 * dot_product(state->eci_sun_hat, state->eci_rso_hat) + 0.5;

    eci_flux.dir = state->eci_rso_hat;

    state->eci_fluxes.push_back(eci_flux);
}
