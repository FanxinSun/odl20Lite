/*! @file Two_line_element.h
	@author David Harrison
	@date 19 April 2016
	@brief SGNL OPS header file defining the class Two_line_element, which is an
		   class for dealing with two-line elements sets.

	Each instance of a Two_line_element object holds the infromation contained
	in a two-line element set. Two-line elements are important in Astrodynamics
	because they widely used within the community. Orbital elements estimated
	from tracking data for a large number of space resident objects are provided
	to users in the the two-line element format. These two-line element sets
	can be found at www.celestrak.com, where there is also comprehensive
	documentation on the two-line element sets.

	<Hoots80> Models for Propagation of NORAD Element Sets, Spacetrack
	          Report no. 3
	<Vallado13> Fundamental of Astrodynamics and Applications, ed. 4,
	      	    Section 2.4.2 Two-line element sets
 */

#ifndef SGNL_TWO_LINE_ELEMENT_H
#define SGNL_TWO_LINE_ELEMENT_H

#include <cstring> // Needed for strncpy
#include <locale>  // Needed for std::isdigit
#include <string>  // Needed for std::string

#include "Keplerian_elements.h"

// Include SGP4 code by Vallardo, see readme PDF for more
#include "../external/sgp4/sgp4io.h"

class Two_line_element
{
  public:
    State_vector current_state;
    State_vector epoch_state;
    sgp4::elsetrec satrec;

    long int sat_number;          // Satellite number
    double mean_motion_1st_deriv; //
    double mean_motion_2nd_deriv; //
    double b_star;                //
    long int tle_set_number;      //

    double inc;
    double raan;
    double ecc;
    double argp;
    double mean;
    double mean_motion;
    long int rev_number;

    // Raw output from all fields of TLE:
    std::string line0;
    std::vector<std::string> line1;
    std::vector<std::string> line2;

    bool is_valid; // Is the TLE object in a valid state?

    Two_line_element(); //default constructor

    Two_line_element(std::string &in_line1, std::string &in_line2);

    Two_line_element(std::string &in_line0, std::string &in_line1,
                     std::string &in_line2);

    bool valid_tle() const;
    bool has_set_leap() const;
    std::string get_name() const;
    State_vector get_current_state() const;
    State_vector get_epoch_state() const;
    bool set_TLE_time(Timetag in_tag);
    bool step_TLE_time(double time_step);
    bool step_TLE_time(long int time_step_int, double time_step_frac);

    void setup(std::string &in_line1, std::string &in_line2);

    void setup(std::string &in_line0, std::string &in_line1,
               std::string &in_line2);

    void init_variables();
    void set_defaults();

    void set_leap_secs(long int in_start_MJDN, long int in_TLE_MJDN);

    long int compute_checksum(std::string &in_line) const;

    void init_sgp4(std::string &in_line1, std::string &in_line2);

    void extract_data(std::string &in_line1, std::string &in_line2);
};

#endif
