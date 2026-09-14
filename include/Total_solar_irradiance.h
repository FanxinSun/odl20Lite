/*! @file Total_solar_irradiance.h
	@author Santosh Bhattarai
	@date 12 March 2015
	@brief SGNL OPS header file defining the class Total_solar_irradiance, which
		   uses a data set to retrieve solar irradiance at a given time.

	Each time the tsi object is instantiated, the time series of solar
	irradiance data held in the file tsi.dat will be read into memory, and
	will be used to determine the total solar irradiance during the time
	period of that data set.

	The Total Solar Irradiance is a quantity that describes the solar radiation
	flux at 1 AU in Watts/m^2.

	<Kopp11> G. Kopp and J. L. Lean. A new, lower value of total solar
	irradiance: Evidence and climate significance. Geophys. Res. Lett.,
	38:L01706, 2011. doi: 10.1029/2010GL045777
 */

#ifndef SGNL_TOTAL_SOLAR_IRRADIANCE_H
#define SGNL_TOTAL_SOLAR_IRRADIANCE_H

#include <fstream>
#include <map>

#include "Timetag.h"

/*
	Each record line of tsi.dat contains timetag information, along with a
	value for TSI computed using the old way, and the value for TSI according
	to the new way.

	N.B tsi_old = 0.9963812 * tsi_new
 */
struct one_tsi_record {
    Timetag epoch;  // Timetag
    double tsi_old; // in W/m2
    double tsi_new; // in W/m2
};

class Total_solar_irradiance
{

  public:
    //!< default value, for times outside of the range of the TSI time series
    //!< based on Kopp11, TSI is ~1361 at solar min and ~1362 at solar max
    double nominal_tsi = 1361.5; //!< W/m2

    //!< key: MJD, record: one_stsi_record
    std::map<double, one_tsi_record> tsi_time_series;

    /*
		When a new instance of tsi is instantiated, with no arguments, e.g.
		tsi new_tsi_instance;
		the tsi.dat file is open, read into memory and TSI time series since
		1978 is stored in the map<> object tsi_time_series;
	 */
    Total_solar_irradiance(); //default constructor

    /*
	getting functions for tsi_new, given a time argument as a double mjd
	or as Timetag object
	 */
    double get_tsi_new(double mjd_in);
    double get_tsi_new(Timetag epoch);
};

#endif
