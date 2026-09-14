/*! @file Total_solar_irradiance.cpp
	@author Santosh Bhattarai
	@date 12 March 2015
	@brief SGNL OPS header file defining the class Total_solar_irradiance, which
		   uses a data set to retrieve solar irradiance at a given time.
 */

#include "../include/Total_solar_irradiance.h"

Total_solar_irradiance::Total_solar_irradiance()
{
    // When a Total_solar_irradiance object is instantiated the tsi_time_series
    // is populated

    // create filestream object and open tsi.dat
    std::string filename = "../res/tsi.dat";
    std::ifstream infile;
    infile.open(filename);

    //std::cout << filename << std::endl;

    // check that the file is open
    if (infile.fail() || !infile.is_open()) {
        std::cerr << "Error opening " << filename << std::endl;
        exit(1);
    }

    int lineCounter = 0;
    std::string line;

    //!< Skipping the headerlines, there are 33 headerlines
    for (int i = 1; i <= 33; i++) {
        lineCounter++;
        std::getline(infile, line);
        //std::cout << line << std::endl;
        line = "";
    }

    while (!infile.eof()) {
        lineCounter++;

        int date;
        double daysSince1980, tsiNew, tsiOld;

        //!< reading in the data records line by line
        infile >> date >> daysSince1980 >> tsiNew >> tsiOld;

        //!< Converting the timetag in the data file into a UCL timetag object
        //!< 1 Jan 1980 is 44239 in MJD
        double mjd_offset = 44238.0;
        double epoch_in_mjd = daysSince1980 + mjd_offset;
        //timetag epoch_of_record = MJD2Cal(epoch_in_mjd);

        //!< storing each line in the struct one_tsi_record
        one_tsi_record one_tsi_line;
        //one_tsi_line.epoch.set_timetag(epoch_of_record);
        one_tsi_line.epoch.set_timetag_from_MJD_TT(epoch_in_mjd);
        one_tsi_line.tsi_old = tsiOld;
        one_tsi_line.tsi_new = tsiNew;

        //!< storing the full time time series in tsi_time_series
        tsi_time_series[epoch_in_mjd] = one_tsi_line;
    }

    infile.close();
}

/*
getting functions for tsi_new, given a time argument as a double mjd
or as Timetag object
 */
double Total_solar_irradiance::get_tsi_new(double mjd_in)
{
    // return value
    double result = 0.0;
    int count = 0; // count needs two neighbours
    double day_before_tsi = 0.0;
    double day_after_tsi = 0.0;
    double day_before_mjd = 0.0;
    double day_after_mjd = 0.0;

    // tolerance - setting a value for compariosn of doubles
    double tol = 1e-6; //!< mjd values in test.dat only to 2 decimal palces

    double first_mjd = tsi_time_series.begin()->first;
    double last_mjd = tsi_time_series.rbegin()->first;

    //if mjd_in falls outside the time series range
    if (mjd_in < first_mjd || mjd_in > last_mjd) {
        return nominal_tsi;
    }

    // now iterate through the map searching for the two records with the
    // closest timetags

    // Note a quicker approach here would be to find the
    for (auto it = tsi_time_series.begin(); it != tsi_time_series.end(); ++it) {

        double tsi_new_value = it->second.tsi_new;
        double mjd_value = it->first;

        //!< if timetags are an exact match, hurrah
        double mjd_comparison = std::abs(mjd_in - mjd_value);
        //double deltaT = mjd_in-mjd_value;

        if (mjd_comparison < tol) {
            result = tsi_new_value;
            //std::cout << "Match found!" << std::endl;
        }
        //!< otherwise need to find the two nearest timetags
        else if (mjd_comparison < 1.0) {
            double scale_factor = 0.0;

            // std::cout << "Upper and lower matches found : "
            // 		  << mjd_value << ", " << tsi_new_value << std::endl;
            count++;

            if (count == 1) {
                day_before_tsi = it->second.tsi_new;
                day_before_mjd = it->first;

                // if the day before is a data gap, find the first day that
                // isn't a data gap, increment backwards
                if (std::abs(static_cast<int>(tsi_new_value)) == 98) {
                    auto it2 = it;
                    while (std::abs(static_cast<int>(it2->second.tsi_new)) ==
                           98) {
                        std::cout << it2->second.tsi_new << std::endl;
                        --it2;
                    }
                    //outside
                    day_before_tsi = it2->second.tsi_new;
                    day_before_mjd = it2->first;
                }
            }
            if (count == 2) {
                day_after_tsi = it->second.tsi_new;
                day_after_mjd = it->first;

                // if the day before is a data gap, find the first day that
                // isn't a data gap, increment forwards
                if (std::abs(static_cast<int>(tsi_new_value)) == 98) {
                    auto it3 = it;
                    while (std::abs(static_cast<int>(it3->second.tsi_new)) ==
                           98) {
                        std::cout << it3->second.tsi_new << std::endl;
                        ++it3;
                    }
                    //outside
                    day_after_tsi = it3->second.tsi_new;
                    day_after_mjd = it3->first;
                }

                double delta_t_mjd_in = day_before_mjd - mjd_in;
                double delta_t_to_day_after = day_before_mjd - day_after_mjd;
                scale_factor = delta_t_mjd_in / delta_t_to_day_after;

                result = day_before_tsi +
                         (scale_factor * (day_after_tsi - day_before_tsi));

                scale_factor = 0; //!< resetting the scale factor
                count = 0;        //!< resetting count
            }
        }
    }

    //std::cout << "result: " << result << std::endl;

    return result;
}
