/*! @file Force_gr_correction.h
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of general relativity corrections to orbits.
 */

#ifndef SGNL_FORCE_GR_CORRECTION_H
#define SGNL_FORCE_GR_CORRECTION_H

#include "Force.h"

/*!
 * @class Force_gr_correction
 * @date 22 February 2016
 * @author David Harrison
 * @brief Computes relativistic correction to satellite acceleration.
 *
 * <Montenbruck00> Satellite Orbits: Model, Methods and Applications,
 *		           Section 3.7.3 Relativistic effects.
 *
 * This is a General Relativistic correction based on Montenbruck, eq 3.146
 * (pg 111, 1st ed.)
 *
 * While not truly a force, it can be modelled as such with a resulting
 * acceleration, in must the same way as centrifugal force can.
 *
 * Note: Uses GM value set by Force_earth_gravity. Force_earth_gravity should be
 * the first of all forces to be instantiated.
 */
class Force_gr_correction : public Force
{
  public:
    double GM = sgnlOPS::GM;

    // A bunch of constants rolled into one
    double minus4GMc2 = -std::round(4000000.0 * GM) / sgnlOPS::c2;

    Force_gr_correction() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;
    void compute_partial_derivatives() const override;
};

#endif
