/*! @file Prop_erk87.h
	@author Santosh Bhattarai
	@date 17 February 2015
	@brief SGNL OPS header file defining an Embedded Runge-Kutta 8(7) Integrator
 */

#ifndef SGNL_PROP_ERK87_H
#define SGNL_PROP_ERK87_H

#include "Propagators.h"

/**
 * @class Prop_erk87
 * @date 17 February 2014
 * @author Santosh Bhattarai
 * @brief Embedded Runge-Kutta 8(7) integrator.
 *
 * This object defines an RK8(7) integrator that operates upon a resident space
 * object. Embedded methods are designed to produce an estimate of the local
 * truncation error of a single Runge-Kutta step, and as result, allow to
 * control the error with adaptive stepsize. This is done by having two methods
 * at the same time, in this case one with order 8 and one with order 7.
 *
 * <Hairer87> Solving Ordinary Differential Equations I: Non-stiff problems
 */
class Prop_erk87 : public Propagators
{
  private:
    long int h_ci[12] = {};
    double h_cf[12] = {};
    long double h_b[12] = {};
    // long double h_bhat[13] = {};
    long double h_a[13][11] = {};

  public:
    Prop_erk87() = default;

    // Member functions
    void step() override;

    void set_step_size(long double step_size) override;
};

#endif
