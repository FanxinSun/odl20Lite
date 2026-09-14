/*! @file Resident_space_object.h
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS header file defining the class Resident_space_object.
 */

#ifndef SGNL_RESIDENT_SPACE_OBJECT_H
#define SGNL_RESIDENT_SPACE_OBJECT_H

#include "Force_antenna_thrust.h"
#include "Force_drag.h"
#include "Force_earth_gravity.h"
#include "Force_earth_tide.h"
#include "Force_gr_correction.h"
#include "Force_lorentz.h"
#include "Force_rp.h"
#include "Force_third_body.h"
#include "Force_trr.h"
#include "Force_trr2.h"
#include "Force_trr3.h"
#include "Force_y_bias.h"

/**
 * @class Resident_space_object
 * @date 16 May 2016
 * @author David Harrison
 * @brief Manages all properties of an object and the forces acting upon it.
 *
 * All active forces are instantiated and stored in a vector of unique_ptr's.
 * This allows easy access to the forces as they all have a common interface,
 * both to setup and to query for the acceleration.
 */
class Resident_space_object
{
  public:
    // phiM is the state transition matrix, 6 by 6
    Matrix6x6 phiM = Matrix6x6::Identity();

    //! srpS is the sensitivity of the state to the solar radiation pressure
    //! scale factor, dy/dp. It starts at zero because the initial state does
    //! not depend on p, and is integrated as d(srpS)/dt = dF/dy * srpS +
    //! [0; da/dp] alongside phiM. Only the propagators that carry phiM
    //! (RKF7/8 and RK4) integrate it.
    Matrix6x5 srpS = Matrix6x5::Zero();

    //! da/dp: the unscaled solar radiation pressure acceleration, in ECI.
    Cartesian get_srp_partial() const;

    //! da/dp for empirical parameter i, in ECI.
    Cartesian get_emp_partial(int i) const;

    //! The radiation pressure scale factor, which the orbit fit estimates.
    double get_srp_scale() const;
    void set_srp_scale(double scale);

    //! ECOM coefficients 1-4 (Y0, B0, Bc, Bs), in km/s^2.
    void set_emp_coeff(int i, double value);

  private:
    std::vector<std::unique_ptr<Force>> forces;
    std::vector<double> acc;
    std::shared_ptr<Resident_variables> state;

  public:
    Keplerian_elements initial_state;
    Two_line_element sgp4_state;

  private:
    Configuration config;
    Resident_constants rso_const;

  public:
    Resident_space_object() = default;

    void setup(const Configuration &in_config);

    std::unique_ptr<Force_earth_gravity>
    gravity_statistics(std::unique_ptr<Force_earth_gravity> grav);

    void compute_partial_derivatives();
    Matrix6x6 get_partial_derivatives() const;
    Matrix6x6 compute_and_get_partial_derivatives();

    // Low precision acceleration estimate, only uses Earth monopole and J2
    // For use in predictor-corrector integrators
    // Should be good to approximately 10^-6 km/s^2, 10^-3 m/s^2 level @ LEO
    // Perhaps as good as 10^-9 km/s^2, 10^-6 m/s^2 level @ MEO and GEO
    Cartesian compute_fast_acceleration(State_vector eci)
    {
        return forces.back()->compute_fast_acceleration(eci);
    }

    // Main function to query all force models, used by propagators
    void compute_acceleration();
    Cartesian get_acceleration() const;
    Cartesian compute_and_get_acceleration();

    std::string accelerations_header() const;
    void
    stream_individual_accelerations(std::stringstream &output_buffer) const;

    void store_max_individual_accelerations();
    void print_max_individual_accelerations();

    std::string solar_properties_header() const;
    void stream_solar_properties(std::stringstream &output_buffer) const;

    std::string atmos_properties_header() const;
    void stream_atmos_properties(std::stringstream &output_buffer) const;

    // Checks for force model errors
    bool is_state_bad() const
    {
        return (state->errors.size() > 0);
    }
    std::string why_bad_state() const;

    // Accessor methods for propagators to interface with Force class
    State_vector get_eci() const
    {
        return state->eci;
    }

    State_vector get_ecef() const
    {
        return state->ecef;
    }

    double get_GM() const
    {
        return rso_const.GM;
    }

    Timetag get_epoch() const
    {
        return state->eci.epoch;
    }

    double get_eclipse_state() const
    {
        return state->eclipse_state;
    }

    void update(State_vector in_state)
    {
        state->update(in_state);
    }

    void update_with_acc(State_vector in_state)
    {
        state->update(in_state);

        compute_acceleration();
    }

    void update_with_acc_and_deriv(State_vector in_state)
    {
        state->update(in_state);

        compute_acceleration();
        compute_partial_derivatives();
    }

    void print_properties() const;
};

#endif
