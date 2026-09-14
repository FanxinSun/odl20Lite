/*! @file Force.cpp
	@author David Harrison
	@date 12 May 2016
	@brief SGNL OPS file defining the Force class.
 */

#include "../include/Force.h"

void Force::compute_partial_derivatives() const
{
}

//! Get the force-model specific ECI acceleration
Cartesian Force::get_eci_acceleration() const
{
    return a_eci;
}

//! Get the force-model specific ECEF acceleration
Cartesian Force::get_ecef_acceleration() const
{
    return a_ecef;
}

//! Get the force-model specific ECI acceleration rotated to the ECEF frame
Cartesian Force::get_eci_acceleration_in_ecef() const
{
    return state->frame_transform.rotate_eci_to_ecef(a_eci);
}

//! Get the force-model specific ECEF acceleration rotated to the ECI frame
Cartesian Force::get_ecef_acceleration_in_eci() const
{
    return state->frame_transform.rotate_ecef_to_eci(a_ecef);
}

//! Get the force-model specific acceleration in the ECI frame
Cartesian Force::get_acceleration_in_eci() const
{
    return a_eci + get_ecef_acceleration_in_eci();
}

//! Get the force-model specific acceleration in the ECEF frame
Cartesian Force::get_acceleration_in_ecef() const
{
    return get_eci_acceleration_in_ecef() + a_ecef;
}

//! Get the magnitude of the force-model specific acceleration
double Force::get_acceleration_magnitude() const
{
    return get_acceleration_in_eci().length();
}
