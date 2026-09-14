/*! @file Force_y_bias.cpp
	@author David Harrison
	@date 5 April 2017
	@brief SGNL OPS implementation of y-bias force.
 */

#include "../include/Force_y_bias.h"

void Force_y_bias::setup(const Resident_constants &rso_const,
                         std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    // Acceleration magnitude in km/s^2
    a_mag = rso_const.y_bias_accel;
}

void Force_y_bias::compute_acceleration()
{
    a_ecef = state->ecef_attitude.y_hat * a_mag;
    state->total_a_ecef += a_ecef;
}

void Force_y_bias::compute_partial_derivatives() const
{
    // We can specify dadr in either the ECI or ECEF frame, but doing it in ECI
    // saves an extra conversion step.
    state->dadr_eci.noalias() += state->eci_attitude.dydr * a_mag;
}
