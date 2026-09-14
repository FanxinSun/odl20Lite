/*! @file Flux_solar_ecef.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS header file defining the Flux_solar class.
 */

#ifndef SGNL_FLUX_SOLAR_ECEF_H
#define SGNL_FLUX_SOLAR_ECEF_H

#include "Flux.h"
#include "Total_solar_irradiance.h"

/*!
 * @class Flux_solar_ecef
 * @date 20 May 2016
 * @author David Harrison
 * @brief Calculates magnitude (in W/m^2) and direction of solar flux in the
 *        ECEF frame.
 */
class Flux_solar_ecef : public Flux
{
  private:
    double au2;
    Total_solar_irradiance tsi;

  public:
    Flux_solar_ecef();

    void add_flux_num(size_t &eci_flux_num,
                      size_t &ecef_flux_num) override;

    void compute_flux() override;
};

#endif
