/*! @file Force_rp_analytic.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of an analytic radiation pressure method.
 */

#ifndef SGNL_FORCE_RP_ANALYTIC_H
#define SGNL_FORCE_RP_ANALYTIC_H

#include "Force_rp.h"

/*!
 * @class Force_rp_analytic
 * @date 20 May 2016
 * @author David Harrison
 * @brief Uses a simple analytic model to calculate acceleration due to incoming
 *        flux (in W/m^2).
 */
class Force_rp_analytic : public Force_rp
{
  private:
    double a_coef = 0.0;



  public:
    Force_rp_analytic() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;
};

#endif
