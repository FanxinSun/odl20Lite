/*! @file Flux_solar_ecef.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux_solar_ecef class.
 */

#include "../include/Flux_solar_ecef.h"

Flux_solar_ecef::Flux_solar_ecef()
{
    // tsi class should be combined with this one so that this variable can be
    // varied depending on solar output
    // tsi = sgnlOPS::solar_flux;
    au2 = sgnlOPS::astronomical_unit *
          sgnlOPS::astronomical_unit; // Distance to sun squared, in km^2
}

void Flux_solar_ecef::add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num)
{
    eci_flux_num += 0;
    ecef_flux_num += 1;
}

void Flux_solar_ecef::compute_flux()
{
    if (state->eclipse_state > 0.0) {
        Fluxstruct ecef_flux;

        // double tsi_flux = tsi.get_tsi_new(state->eci.epoch.get_MJD_UTC());

        double tsi_flux = sgnlOPS::solar_flux;

        ecef_flux.mag_sw = tsi_flux * au2 / state->rso_sun_distance2;

        // Scaling of solar flux based on eclipse state
        ecef_flux.mag_sw *= state->eclipse_state;

        // Assume 0.5% of all solar flux is long wave (>5000 nm wavelength)
        ecef_flux.mag_lw = ecef_flux.mag_sw * 0.005;
        ecef_flux.mag_sw -= ecef_flux.mag_lw;

        ecef_flux.dir = -state->ecef_rso_sun_hat;

        state->ecef_fluxes.push_back(ecef_flux);
    }
}
