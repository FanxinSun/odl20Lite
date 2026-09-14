/*! @file Prop_sgp4.h
	@author David Harrison
	@date 5 May 2016
	@brief SGNL OPS file defining an SPG4 propagator for Two Line Elements.
 */

#ifndef SGNL_PROP_SGP4_H
#define SGNL_PROP_SGP4_H

#include "Propagators.h"

/**
 * @class Prop_sgp4
 * @date 5 May 2016
 * @author David Harrison
 * @brief SGP4 propagator using methods built into the Two_line_element class.
 */
class Prop_sgp4 : public Propagators
{
  public:
    Prop_sgp4() = default;

    // Member functions
    void step() override;
};

#endif
