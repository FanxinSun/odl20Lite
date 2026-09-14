/*! @file Prop_kepler.cpp
	@author David Harrison
	@date 14 March 2016
	@brief SGNL OPS file defining a basic keplerian propagator.
 */

#include "../include/Prop_kepler.h"

void Prop_kepler::step()
{
    Keplerian_elements new_el = rso.initial_state;

    new_el.epoch = rso.get_epoch();
    new_el.epoch.step(h_i, h_f);

    long double sqrtGMa3 = std::sqrt(new_el.GM / new_el.sma) / new_el.sma;

    long double t =
        static_cast<long double>(new_el.epoch - rso.initial_state.epoch);

    // Compute the mean anomaly at the required point
    long double new_mean = new_el.mean + t * sqrtGMa3;

    new_el.set_mean(new_mean);

    rso.update(new_el);
}
