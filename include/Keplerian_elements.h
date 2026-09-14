/*! @file Keplerian_elements.h
	@author David Harrison
	@date 21 March 2016
	@brief SGNL OPS header file defining the object Keplerian_elements.
 */

#ifndef SGNL_KEPLERIAN_ELEMENTS_H
#define SGNL_KEPLERIAN_ELEMENTS_H

#include "State_vector.h"

/**
 * @class Keplerian_elements
 * @author David Harrison
 * @date 21 March 2016
 *
 * @brief This is a class that deals with orbits represeted in the Keplerian
 *		  elements format, and has functions for converting to and from an ECI
 *        State_vector representation.
 */
class Keplerian_elements : public State_vector
{
  public:
    // Attributes
    long double sma;  ///!< semi-major axis (km)
    long double ecc;  ///!< eccentricity of ellipse (ratio-->no units)
    long double inc;  ///!< inclination of orbital plane (rad)
    long double argp; ///!< argument of perigee (rad)
    long double raan; ///!< right ascension of ascending node (rad)
    long double tran; ///!< true anomaly (rad)
    long double ecan; ///!< eccentric anomaly (rad)
    long double mean; ///!< mean anomaly (rad)

    long double GM; ///!< gravitational parameter (km^3 / s^2)

    Keplerian_elements(); ///!< default constructor

    ///!< keplerian elements constructor, note epoch is last argument
    Keplerian_elements(long double in_sma, long double in_ecc,
                       long double in_inc, long double in_argp,
                       long double in_raan, long double in_tran,
                       long double in_GM, Timetag in_epoch);

    ///!< state vector constructor
    Keplerian_elements(State_vector in_state, long double in_GM);

    ///!< state vector elements constructor, note GM is last argument
    Keplerian_elements(long double in_x, long double in_y, long double in_z,
                       long double in_u, long double in_v, long double in_w,
                       Timetag in_epoch, long double in_GM);

    // Methods
    void set(long double in_sma, long double in_ecc, long double in_inc,
             long double in_argp, long double in_raan, long double in_tran,
             long double in_GM, Timetag in_epoch);

    void set(State_vector in_state);

    void set(long double in_x, long double in_y, long double in_z,
             long double in_u, long double in_v, long double in_w,
             Timetag in_epoch);

    // Set arg of peri, useful for correcting parameter in circular orbits
    void set_argp(long double in_argp, bool update_state_vec);

    // Set RAAN, useful for correcting parameter in equatorial orbits
    void set_raan(long double in_raan, bool update_state_vec);

    // Set true anomaly, updates eccentric and mean anomalies and state vector
    void set_tran(long double in_tran);

    // Set eccentric anomaly, updates true and mean anomalies and state vector
    void set_ecan(long double in_ecan);

    // Set mean anomaly, updates true and eccentric anomalies and state vector
    void set_mean(long double in_mean);

    /**
		 * @fn update_state_vector (originally kep2cart)
		 * @author David Harrison
		 * @brief Converts Keplerian elements to ECI cartesian state vector
		 *
		 * Calculates ECI position and velocity elements of the state vector
		 * from Keplerian elements.
		 *
		 * Originally written by Marek Ziebart, this method has since been
		 * rewritten from scratch. Since it is functionally very similar, the
		 * original comments have been left.
		 */
    void update_state_vector();

    /**
		 * @fn update_elements (originally cart2kep)
		 * @author Marek Ziebart
		 * @brief Converts ECI cartesian state vector to Keplerian elements.
		 * Vallado13. RV2COE (Algorithm 9) in Fundamentals of
		 *		      Astrodynamics and Applications. 4th Edition.
		 * Also useful: https://downloads.rene-schwarz.com/download/M002-
		 * 			    Cartesian_State_Vectors_to_Keplerian_Orbit_Elements.pdf
		 *
		 * Note: Inputs should be position (km), velocity (kms-1)
		 *	     Outputs are in km, and radians
		 *
		 * Cartesian to Keplerian elements conversion routine. The cartesian
		 * state vector has to be in Earth-Centred Inertial coordinates. Note
		 * that this routine is designed for the analysis of satellites in
		 * elliptical orbits gravitationally bound to the Earth. If the
		 * trajectory is parabolic or hyperbolic the function will bomb.
		 *
		 * Updates to this function have been made by David Harrison so that for
		 * circular or equatorial orbits, no angles will be set to NaN.
		 */
    void update_elements();

    // Return string representations of the object
    std::string str() const override;
    std::string str_state_vector() const;
    std::string str_elements() const;

    // Prints the corresponding string representation
    void print() const override;
    void print_state_vector() const;
    void print_elements() const;

  private:
    void update_mean_from_ecan();

    void update_tran_from_ecan();

    void update_ecan_from_tran();
    void update_ecan_from_mean();
};

#endif
