/*! @file JPL_orbit_format.cpp
 	@author Santosh Bhattarai
 	@date 23 March 2017
 	@brief SGNL OPS class file defining the object JPL_orbit_format which
 	       processes orbit data stored in the JPL orbit eci format:

 	       ID(I2) Y(I4) M(I2) D(I2) H(I2) M(I2) S(D2.2) // continues next line
 	       X(D.6) Y(D.6) Z(D.6) U(D.6) V(D.6) W(D.6)
 */

#include "../include/JPL_orbit_format.h"

JPL_orbit_format::JPL_orbit_format(const std::vector<std::string> &orbit_files)
{
    process_files(orbit_files);
}

void JPL_orbit_format::process_files(
    const std::vector<std::string> &orbit_files)
{
    // Loop through orbit files
    for (const auto &file : orbit_files) {

        // Create filestream object and open orbit file
        std::ifstream infile(file);

        // Check that the file is open
        if (infile.fail() || !infile.is_open()) {
            std::cerr << "Error opening " << file << std::endl;
            exit(1);
        }

        std::cout << "Reading " << file << std::endl;

        int line_counter = 0;
        std::string line;

        // Variables for storing JPL orbit record
        int sat_id;
        int year, month, day, hr, min;
        double sec;
        State_vector sat_state;

		// This test will also catch empty files so we don't read nonsense in
        while (infile >> sat_id) {
            line_counter++; // Just check orbit_time_series.size() instead?

            // Read in the line
            infile >> year >> month >> day >> hr >> min >> sec >> sat_state.x >>
                sat_state.y >> sat_state.z >> sat_state.u >> sat_state.v >>
                sat_state.w;

            // Set the timetag
            sat_state.epoch.set_timetag_from_cal_UTC(year, month, day, hr, min,
                                                     sec);

            // Debugging: check the lines have been read in properly
            // std::cout << sat_id << ", " << year << ", " << month << ", " << day
            //           << ", " << hr << ", " << min << ", " << sec << ", "
            //           << sat_state.x << ", " << sat_state.y << ", "
            //           << sat_state.z << ", " << sat_state.u << ", "
            //           << sat_state.v << ", " << sat_state.w << std::endl;

            // Add orbit information to orbit_time_series
            orbit_time_series.push_back(sat_state);

			// Go to next line
            std::getline(infile, line);
        }

        satellite_id = std::to_string(sat_id);
        //std::cout << "Satellite id test: " << satellite_id << "\n";

        infile.close();
    }
}
