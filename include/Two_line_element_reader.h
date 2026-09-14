/*! @file Two_line_element_reader.h
	@author David Harrison
	@date 5 June 2017
	@brief Process CelesTrak JSON data files and store the TLEs as a vector.
 */

#ifndef SGNL_TWO_LINE_ELEMENT_READER_H
#define SGNL_TWO_LINE_ELEMENT_READER_H

#include <algorithm>
#include <fstream>
#include <vector>

#include "Two_line_element.h"

struct layerstruct {
    double altitude = 0.0;
    double height = 0.0;
    double d[180][360] = {};
};

class Two_line_element_reader
{
  private:
    static constexpr double R = 6371.0; // Average radius of the earth in km

    Two_line_element_reader() = default;

  public:
    static std::vector<Two_line_element>
    read_and_sort_TLE(const std::vector<std::string> &files, Timetag epoch);

    static std::vector<Keplerian_elements>
    get_unique_TLE_orbits(const std::vector<Two_line_element> &TLE);

    static std::vector<layerstruct>
    get_spatial_density(const std::vector<Keplerian_elements> &orbits,
                        double altitude_start, double layer_height,
                        size_t num_layers, int orbit_slice);

    static size_t alt_bin(double r, double altitude_floor, double layer_height);
    static size_t lat_bin(Cartesian pos, double r);
    static size_t lon_bin(Cartesian pos);
};

#endif
