/*! @file Flux_solar_eci.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux_solar class.
 */

#include "../include/Flux_solar_eci.h"

Flux_solar_eci::Flux_solar_eci()
{
    // tsi class should be combined with this one so that this variable can be
    // varied depending on solar output
    // tsi = sgnlOPS::solar_flux;
    au2 = sgnlOPS::astronomical_unit *
          sgnlOPS::astronomical_unit; // Distance to sun squared, in km^2
}

void Flux_solar_eci::add_flux_num(size_t &eci_flux_num, size_t &ecef_flux_num)
{
    eci_flux_num += 1;
    ecef_flux_num += 0;
}

void Flux_solar_eci::compute_flux()
{
    if (state->eclipse_state > 0.0) {
        Fluxstruct eci_flux;

        // double tsi_flux = tsi.get_tsi_new(state->eci.epoch.get_MJD_UTC());

        double tsi_flux = sgnlOPS::solar_flux;

        eci_flux.mag_sw = tsi_flux * au2 / state->rso_sun_distance2;

        // Scaling of solar flux based on eclipse state
        eci_flux.mag_sw *= state->eclipse_state;

        // Assume 0.5% of all solar flux is long wave (>5000 nm wavelength)
        eci_flux.mag_lw = eci_flux.mag_sw * 0.005;
        eci_flux.mag_sw -= eci_flux.mag_lw;

        eci_flux.dir = -state->eci_rso_sun_hat;

        state->eci_fluxes.push_back(eci_flux);
    }
}
