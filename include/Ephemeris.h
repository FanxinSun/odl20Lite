/*! @file Ephemeris.h
	@author David Harrison
	@date 13 May 2016
	@brief SGNL OPS header file defining the object Ephemeris, which serves as
		   an interface to the ephemeris functions in astrolib.cpp

    Originally based on code by Joe Heafner named sephem.
 */

#ifndef SGNL_EPHEMERIS_H
#define SGNL_EPHEMERIS_H

#include "Cartesian.h"
#include "Timetag.h"

// Include Ephemeris code by Joe Heafner, see included PDF for more
#include "../external/fecsoft/astrolib.h"

struct Planetstruct {
    double GM = 0.0;
    Cartesian pos;
};

//TODO: Validate DE405 against JPL Telnet server look at DE430?
/**
 * @class Ephemeris
 * @author David Harrison
 * @date 13 May 2016
 *
 * @brief This is a class that interfaces with the Ephemeris functions in
 *        astrolib.cpp to determine the positions of planets at a given epoch.
 *        Positions are stored as an array of Cartesian objects.
 *
 * 		***************************************************************
 * 			The UCL numbering convention is:
 * 				0 = Sun			5 = Jupiter
 * 				1 = Mercury		6 = Saturn
 * 				2 = Venus       7 = Uranus
 * 				3 = Moon        8 = Neptune
 * 				4 = Mars        9 = Pluto
 * 			THIS SHOULD BE ADHERED TO WHEN RETRIEVING POSITION INFO
 * 			FROM THE DATA STRUCTURE
 * 		***************************************************************
 */
class Ephemeris
{
  private:
    Cartesian r_earth;
    Cartesian v_earth;
    // Whether ephemeris is calculated for each body
    std::vector<bool> enabled = std::vector<bool>(10, false);

  public:
    // GM and Position of enabled bodies
    std::vector<Planetstruct> bodies;

    // Term used for GR correction "force"
    Cartesian helio_V_cross_R_term;

    /* Note that there are TWO differing positions for the sun and moon, one for
     * SRP/eclipse calculations and one for third body gravity calculations. The
     * apparent position of the sun and moon takes into account relativistic
     * aberration effects that are not applicable for gravity.
     */
    // Commented out as currently unused:
    // Cartesian solar_pos; //!< Position of Sun for SRP calculations
    // Cartesian lunar_pos; //!< Position of Moon for eclipse calculations

  private:
    Fecsoft fecsoft;

  public:
    Ephemeris() = default;
    Ephemeris(const std::vector<bool> &solar_system_bodies);

    // Rule of 5 since we need to handle the Fecsoft class carefully
    // See here for details: https://stackoverflow.com/a/3279550
    Ephemeris(const Ephemeris &in);     // Copy constructor
    Ephemeris &operator=(Ephemeris in); // Copy assignment
    Ephemeris(Ephemeris &&in);          // Move constructor
    // Move assignment operator is created automagically

    ~Ephemeris() = default;

  private:
    void resize_output_vector();

  public:
    void enable_bodies(const std::vector<bool> &solar_system_bodies);

    double converge_TDB_TT(Cartesian eci_rso, Timetag epoch);
    double compute_ephemeris(Cartesian eci_rso, Timetag epoch);

    friend void ephem_swap(Ephemeris &lhs, Ephemeris &rhs);
};

#endif
