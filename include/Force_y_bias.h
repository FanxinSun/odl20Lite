/*! @file Force_y_bias.h
	@author David Harrison
	@date 5 April 2017
	@brief SGNL OPS implementation of y-bias force.

 */

#ifndef SGNL_FORCE_Y_BIAS_H
#define SGNL_FORCE_Y_BIAS_H

#include "Force.h"

/*!
 * @class Force_y_bias
 * @date 13 May 2016
 * @author David Harrison
 * @brief Calculates acceleration due to antenna power in the body frame
 *        negative z direction. Assumes that the antenna points directly
 *        along the positive z direction.
 */
class Force_y_bias : public Force
{
  public:
    double a_mag = 0.0; // Left public for ease of future debugging

    Force_y_bias() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;
    void compute_partial_derivatives() const override;
};

#endif
