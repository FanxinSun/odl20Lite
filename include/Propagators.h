/*! @file Propagators.h
    @author David Harrison
    @date 14 March 2016
    @brief SGNL OPS header file defining a factory class to select propagator.
 */

#ifndef SGNL_PROPAGATORS_H
#define SGNL_PROPAGATORS_H

#include <chrono>

#include "Output_handler.h"
#include "Resident_space_object.h"

class Propagators
{
  protected:
    long int h_i = 0;        //!< Integer part of step size (in seconds)
    double h_f = 0.0;        //!< Fractional part of step size (in seconds)
    double h = 0.0;          //!< Step size (in seconds)
    long double h_LD = 0.0L; //!< Step size (in seconds) as a long double

    double total_time = 0.0;
    double output_time = 0.0;
    long int total_steps = 0;
    long int output_steps = 0;

  public:
    Resident_space_object rso;
    Output_handler output;

    static std::unique_ptr<Propagators> get_prop(const Configuration &config,
                                                 std::string outfile);

    virtual void set_step_size(long double step_size);

    virtual void setup(const Configuration &config, std::string outfile);

    void propagate_and_print_time();
    virtual double propagate();
    virtual void step() = 0;

    virtual ~Propagators() = default;

  private:
    // Rule of 5 since we need a virtual destructor
    Propagators(const Propagators &copy_from) = default;
    Propagators &operator=(const Propagators &copy_from) = default;
    Propagators(Propagators &&) = default;
    Propagators &operator=(Propagators &&) = default;

  protected:
    Propagators() = default;
};

// These includes are at the end because the classes in them extend this one.
// The get_prop() method in this class refers to them.
#include "Prop_erk87.h"
#include "Prop_erkf78.h"
#include "Prop_kepler.h"
#include "Prop_rk4.h"
#include "Prop_sgp4.h"

#endif
