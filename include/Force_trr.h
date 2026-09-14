/*! @file Force_trr.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of a solar panel Thermal Re-Radiation model.
 */

#ifndef SGNL_FORCE_TRR_H
#define SGNL_FORCE_TRR_H

#include "Force.h"

/**
 * @class Force_trr
 * @date 14 Sep 2015
 * @author Santosh Bhattarai
 * @brief Calculates panel temperatures from flux data, then uses those to
 *        calculate acceleration due to thermal re-radiation from the panels.
 *
 * Must be run AFTER all radiation forces that update the solar_array fluxes.
 */
class Force_trr : public Force
{
  public:
    double temp[3] = {315.0, 315.0, 315.0};
    double area = 0.0;
    double alpha[2] = {};
    double k[2] = {};
    double epsilon[2] = {};
    double delta[2] = {};
    double commanded_power = 0.0;
    double a_coef = 0.0;

    Force_trr() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

  private:
    void panel_surface_temps();
};

#endif
