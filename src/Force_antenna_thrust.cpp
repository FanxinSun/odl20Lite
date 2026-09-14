/*! @file Force_antenna_thrust.cpp
	@author David Harrison
	@date 13 May 2016
	@brief SGNL OPS implementation of antenna thrust force.
 */

#include "../include/Force_antenna_thrust.h"

void Force_antenna_thrust::setup(const Resident_constants &rso_const,
                                 std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    // Acceleration magnitude in km/s^2
    a_mag = (rso_const.antenna_power / (-1000.0 * rso_const.mass * sgnlOPS::c));

    if (rso_const.antenna_power <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_antenna_thrust: antenna power is 0 or negative, "
              << rso_const.antenna_power << " W.";
        state->errors.push_back(error.str());
    }

    if (rso_const.mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_antenna_thrust: spacecraft mass is 0 or negative, "
              << rso_const.mass << " kg.";
        state->errors.push_back(error.str());
        a_mag = 0.0; // Prevent future NaN's
    }
}

void Force_antenna_thrust::compute_acceleration()
{
    a_ecef = state->ecef_attitude.z_hat * a_mag;
    state->total_a_ecef += a_ecef;
}

void Force_antenna_thrust::compute_partial_derivatives() const
{
    // We can specify dadr in either the ECI or ECEF frame, but doing it in ECI
    // saves an extra conversion step.
    state->dadr_eci.noalias() += state->eci_attitude.dzdr * a_mag;
}
