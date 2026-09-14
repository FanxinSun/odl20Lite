/*! @file Force_trr3.h
	@author David Harrison
	@date 21 May 2016
	@brief SGNL OPS implementation of a solar panel Thermal Re-Radiation model.
 */

#ifndef SGNL_FORCE_TRR3_H
#define SGNL_FORCE_TRR3_H

#include "Force.h"

/**
 * @class Force_trr3
 * @date 21 May 2016
 * @author David Harrison
 * @brief Calculates panel temperatures from flux data, then uses those to
 *        calculate acceleration due to thermal re-radiation from the panels.
 *
 * Calculates TRR with a slightly different model than for Force_trr.
 *
 * Must be run AFTER all radiation forces that update the solar_array fluxes.
 */
class Force_trr3 : public Force
{
  public:
    double alpha[2] = {};
    double epsilon[2] = {};
    double sigep[2] = {};
    double k_x[2] = {};
    double k_x2[2] = {};
    double powered_area = 0.0;
    double commanded_power_den = 0.0;
    double a_coef = 0.0;

    Force_trr3() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

  private:
    double emitted_power_density(double E0, double E1, double E2);
};

#endif
