/*! @file SP3.h
    @author Santosh Bhattarai
    @date 6 May 2017
    @brief Process SP3 files and store the in a data structure
 */

#ifndef SGNL_SP3_H
#define SGNL_SP3_H

#include <algorithm>
#include <fstream>
#include <map>
#include <vector>

#include "Cartesian.h"
#include "Timetag.h"
#include "lagrange_interpolation.h"

struct sp3_record {
    Timetag epoch;
    std::string sat_id;
    Cartesian sat_position_ecef;
    Cartesian sat_velocity_ecef;

    double clock_offset = 0.0;
    double clock_bias = 0.0;

    bool clock_event = false;
    bool predicted_clock = false;
    bool predicted_orbit = false;
    bool manuever = false;
};

/* @class SP3
 * @author Santosh Bhattarai
 * @date 6 July 2017
 * @brief A bare bones SP3 reader to extract orbit information.
 */
class SP3
{
  public:
    std::vector<std::string> sp3_list;

    // Stores the data in the way it is given in the SP3 files:
    // Time -> All SV -> Orbit record for each SV
    std::map<Timetag, std::map<std::string, sp3_record>> all_sp3_data;

    // Sat Id -> Full time series of orbit data for each satellite
    // Where velocity information is not provided in the raw SP3 data files
    // they can be
    std::map<std::string, std::vector<sp3_record>> all_sp3_time_series;

    // default constructor
    SP3();

    void read_SP3();

    void get_sp3_time_series();
};

#endif
