/*! @file Force_earth_tide.h
	@author Santosh Bhattarai
	@date 22 June 2017
	@brief SGNL OPS header file dealing with the effects of solid Earth tide.

    For the moment, the implementation of the solid earth tide is based on the
    old UCL libary code, i.e. an implementation of the IERS Conventions 2003.

    To-do: Look into the IERS Conventions 2010 recommendations for dealing with
    this effect.
 */

#ifndef SGNL_FORCE_EARTH_TIDE_H
#define SGNL_FORCE_EARTH_TIDE_H

#include "Force.h"

/*!
 * @class Force_earth_tide
 * @date 28 June 2017
 * @author Santosh Bhattarai
 * @brief Solid Earth tide.
 */
class Force_earth_tide : public Force
{
  public:
    Force_earth_tide() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;
    void compute_partial_derivatives() const override;
};

#endif
