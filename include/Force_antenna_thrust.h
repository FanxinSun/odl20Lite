/*! @file Force_antenna_thrust.h
	@author David Harrison
	@date 13 May 2016
	@brief SGNL OPS implementation of antenna thrust force.

    Ziebart et al. Computation of Solar Radiation Pressure and Thermal
    Re-radiation models for the GPS IIR spacecraft, Report to the US
    Air force, 2004.

	NB. Although the form of the antenna thrust equation is a
	relatively simple one, a separate antenna thrust module is created
	here - as there are cases where it may not be so simple to determine
	the inputs to the antenna thrust equations. E.g. the SAR antenna on
	Sentinel-1 is transits its radar signal in bursts, and is on for
	about 30% of the time. So this complicates the way in which the
	power input is provided to the antenna thrust function. One approach
	could be to use mean signal transmit power.

    NB. This function models the force due to a transmitter point in the
        body frame z-hat direction - it will have to modified to make it
        more generally applicable.
 */

#ifndef SGNL_FORCE_ANTENNA_THRUST_H
#define SGNL_FORCE_ANTENNA_THRUST_H

#include "Force.h"

/*!
 * @class Force_antenna_thrust
 * @date 13 May 2016
 * @author David Harrison
 * @brief Calculates acceleration due to antenna power in the body frame
 *        negative z direction. Assumes that the antenna points directly
 *        along the positive z direction.
 */
class Force_antenna_thrust : public Force
{
  public:
    double a_mag = 0.0; // Left public for ease of future debugging

    Force_antenna_thrust() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;
    void compute_partial_derivatives() const override;
};

#endif
