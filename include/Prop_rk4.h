/*! @file Prop_rk4.h
	@author David Harrison
	@date 19 January 2017
	@brief SGNL OPS header file defining a Runge-Kutta 4 integrator.
 */

#ifndef SGNL_PROP_RK4_H
#define SGNL_PROP_RK4_H

#include "Propagators.h"

/**
 * @class Prop_rk4
 * @date 19 January 2017
 * @author David Harrison
 * @brief Simple Runge-Kutta 4 integrator using the 3/8 rule.
 *
 * This object defines an RK4 integrator that operates upon a resident space
 * object.
 *
 * <Hairer2008> Solving Ordinary Differential Equations I: Non-stiff problems,
 *              page 138
 */
class Prop_rk4 : public Propagators
{
  private:
    long int h_ci[4] = {};
    double h_cf[4] = {};
    long double h_b[4] = {};
    long double h_a[4][3] = {};

  public:
    Prop_rk4() = default;

    // Member functions
    void step() override;

    void set_step_size(long double step_size) override;
};

#endif
