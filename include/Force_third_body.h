/*! @file Force_third_body.h
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS header file defining the acceleration due to gravitational
		   effects of the sun, moon and solar system planets.
 */

#ifndef SGNL_FORCE_THIRD_BODY_H
#define SGNL_FORCE_THIRD_BODY_H

#include "Force.h"

/*!
 * @class Force_third_body
 * @date 30 November 2015
 * @author David Harrison
 * @brief Computes accelerations (in ECI) due to the Sun, moon and the solar
 *        system planets (and pluto).
 *
 * <Montenbruck00> Satellite Orbits: Model, Methods and Applications,
 *		           Section 3.2 Geopotential.
 *
 * By default: Sun, Venus, Moon, and Jupiter are on, others off.
 */
class Force_third_body : public Force
{
  public:
    Force_third_body() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_partial_derivatives() const override;

    void compute_acceleration() override;
};

#endif
