/*! @file Flux.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS file defining the Flux class.
 */

#include "../include/Flux.h"

std::vector<std::unique_ptr<Flux>> Flux::get_flux_models(int solar_flux_model,
                                                         int earth_flux_model)
{
    std::vector<std::unique_ptr<Flux>> flux_models;

    // Big enough to hold one of each type of flux model, prevents reallocation.
    flux_models.reserve(2);

    // Only allow 1 Earth flux model to be active at a time:
    if (earth_flux_model == 1) {
        flux_models.push_back(std::make_unique<Flux_earth_ecef>());
    } else if (earth_flux_model == 2) {
        flux_models.push_back(std::make_unique<Flux_earth_eci>());
    } else if (earth_flux_model == 3) {
        flux_models.push_back(std::make_unique<Flux_earth_ceres>());
    }

    // Only allow 1 Solar flux model to be active at a time:
    if (solar_flux_model == 1) {
        flux_models.push_back(std::make_unique<Flux_solar_ecef>());
    } else if (solar_flux_model == 2) {
        flux_models.push_back(std::make_unique<Flux_solar_eci>());
    }

    return flux_models;
}

void Flux::setup(const std::shared_ptr<Resident_variables> &in_state)
{
    state = in_state;
}
