/*! @file JPL_orbit_format.h
 	@author Santosh Bhattarai
 	@date 23 March 2017
 	@brief SGNL OPS header file defining the object JPL_orbit_format which
 	       processes orbit data stored in the JPL orbit eci format:

 	       ID(I2) Y(I4) M(I2) D(I2) H(I2) M(I2) S(D2.2) // continues next line
 	       X(D.6) Y(D.6) Z(D.6) U(D.6) V(D.6) W(D.6)
 */
#ifndef SGNL_JPL_ORBIT_FORMAT_H
#define SGNL_JPL_ORBIT_FORMAT_H

#include <fstream>
#include <string>

#include "Keplerian_elements.h"

/*!
 * @class JPL_orbit_format
 * @date 24 March 2017
 * @author Santosh Bhattarai
 * @brief Class that deals with reading and storing orbit information given
 *		  in the JPL orbit format.
 */
class JPL_orbit_format
{
  public:
    // stores orbit information provided in a JPL orbit format
    std::vector<State_vector> orbit_time_series;
    std::string satellite_id;

    JPL_orbit_format() = default;
    JPL_orbit_format(const std::vector<std::string> &orbit_files);

    void process_files(const std::vector<std::string> &orbit_files);
};

#endif
