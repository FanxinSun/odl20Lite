/*! @file Prop_kepler.h
	@author David Harrison
	@date 14 March 2016
	@brief SGNL OPS header file defining a basic keplerian propagator.
 */

#ifndef SGNL_PROP_KEPLER_H
#define SGNL_PROP_KEPLER_H

#include "Propagators.h"

/**
 * @class Prop_kepler
 * @date 14 March 2016
 * @author David Harrison
 * @brief Basic Keplerian propagator.
 *
 * This object defines a basic keplerian propagator that operates upon a
 * resident space object.
 */
class Prop_kepler : public Propagators
{
  public:
    Prop_kepler() = default;

    // Member functions
    void step() override;
};

#endif
