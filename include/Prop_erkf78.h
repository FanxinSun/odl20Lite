/*! @file Prop_erkf78.h
    @author Zhen Li
    @date 15 November 2016
    @brief SGNL OPS header file defining an Embedded Runge-Kutta-Fehlberg 7(8)
           Integrator
 */

#ifndef SGNL_PROP_ERKF78_H
#define SGNL_PROP_ERKF78_H

#include "Propagators.h"

/**
 * @class Prop_erkf78
 * @date 15 November 2016
 * @author Zhen Li
 * @brief Embedded Runge-Kutta-Fehlberg 7(8) integrator.
 *
 * This object defines an RKF7(8) integrator that operates upon a resident space
 * object. Embedded methods are designed to produce an estimate of the local
 * truncation error of a single Runge-Kutta-Fehlberg step, and as result, allow
 * to control the error with adaptive stepsize. This is done by having two
 * methods at the same time, in this case one with order 7 and one with order 8.
 *
 * <Hairer87> Solving Ordinary Differential Equations I: Non-stiff problems
 */
class Prop_erkf78 : public Propagators
{
  private:
    static const int m_bCol;
    static const int m_bRow;
    static const double m_c[13];
    static const double m_b[13];
    static const double m_bhat[13];
    static const double m_a[13][12];

  public:
    Prop_erkf78() = default;

    // Member functions
    void step() override;
};

#endif
