/*! @file Flux_earth_eci.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS header file defining the Flux_earth_eci class.
 */

#ifndef SGNL_FLUX_EARTH_ECI_H
#define SGNL_FLUX_EARTH_ECI_H

#include "Flux.h"

/*!
 * @class Flux_earth_eci
 * @date 20 May 2016
 * @author David Harrison
 * @brief Calculates magnitude (in W/m^2) and direction of earth flux in the
 *        ECI frame.
 */
class Flux_earth_eci : public Flux
{
  private:
    double ceres_earth_rad2;

    double avg_earth_flux_lw;
    double avg_earth_flux_sw;

  public:
    Flux_earth_eci();

    void add_flux_num(size_t &eci_flux_num,
                      size_t &ecef_flux_num) override;

    void compute_flux() override;
};

#endif
