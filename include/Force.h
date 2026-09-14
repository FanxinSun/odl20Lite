/*! @file Force.h
	@author David Harrison
	@date 12 May 2016
	@brief SGNL OPS header file defining the Force class.
 */

#ifndef SGNL_FORCE_H
#define SGNL_FORCE_H

#include "Flux.h"
#include "Resident_constants.h"

/*!
 * @class Force
 * @date 12 May 2016
 * @author David Harrison
 * @brief Abstract base class for all force models.
 */
class Force
{
  protected:
    /*
	 * Basic Force members.
	 */
    Cartesian a_eci;  // Force model specific accelerations
    Cartesian a_ecef; // Force model specific accelerations

    std::shared_ptr<Resident_variables> state; // Variables for force models

    Force() = default;

  private:
    // Rule of 5 since we need a virtual destructor
    Force(const Force &copy_from) = default;
    Force &operator=(const Force &copy_from) = default;
    Force(Force &&) = default;
    Force &operator=(Force &&) = default;

  public:
    /*
	 * Member functions
	 */
    virtual ~Force() = default;

    virtual void setup(const Resident_constants &rso_const,
                       std::shared_ptr<Resident_variables> in_state) = 0;

    virtual void compute_partial_derivatives() const;

    /*!
     * Force model specific compute acceleration function, this should compute
     * and store the accelerations in a_eci and/or a_ecef, AND update
     * total_a_eci and/or total_a_ecef in state.
     */
    virtual void compute_acceleration() = 0;

    virtual Cartesian compute_fast_acceleration(State_vector eci) const
    {
        sgnlOPS::ignore(eci);
        return Cartesian();
    }

    //! Get the force-model specific acceleration.
    Cartesian get_eci_acceleration() const;
    Cartesian get_ecef_acceleration() const;
    Cartesian get_eci_acceleration_in_ecef() const;
    Cartesian get_ecef_acceleration_in_eci() const;
    Cartesian get_acceleration_in_eci() const;
    Cartesian get_acceleration_in_ecef() const;
    double get_acceleration_magnitude() const;
};

#endif
