/*! @file Output_handler.cpp
	@author David Harrison
	@date 16 July 2017
	@brief Output_handler is responsible for writing simulation data to a file.
 */

#include "../include/Output_handler.h"

std::string Output_handler::header_full_state()
{
    // clang-format off
	return "Time (s)" ", "
           "     MJD UTC     " ", "
           "     ECI X (km)    " ", "
           "     ECI Y (km)    " ", "
           "     ECI Z (km)    " ", "
           "     ECI U (km/s)    " ", "
           "     ECI V (km/s)    " ", "
           "     ECI W (km/s)    " ", "
           " Semi-Major Axis (km)" ", "
           "     Eccentricity     " ", "
           "   Inclination (rad)  " ", "
           " Arg of Perigee (rad) " ", "
           "      RAAN (rad)      " ", "
           "  True Anomaly (rad)  ";
    // clang-format on
}

void Output_handler::stream_full_state(const Resident_space_object &rso)
{
    Keplerian_elements full_state(rso.get_eci(),
                                  static_cast<long double>(rso.get_GM()));

    output_buffer.precision(19);

    double sim_time = full_state.epoch - rso.initial_state.epoch;

    // clang-format off
	output_buffer << std::setfill(' ') << std::setw(8) << sim_time
                  << std::setprecision(11) << std::fixed << std::right
                  << ", " << std::setw(17) << full_state.epoch.get_MJD_UTC()
                  << std::setprecision(12)
                  << ", " << std::setw(19) << full_state.x
			      << ", " << std::setw(19) << full_state.y
			      << ", " << std::setw(19) << full_state.z
			      << std::setprecision(18)
                  << ", " << std::setw(21) << full_state.u
			      << ", " << std::setw(21) << full_state.v
			      << ", " << std::setw(21) << full_state.w

			      << std::setprecision(15)
                  << ", " << std::setw(21) << full_state.sma
			      << std::setprecision(20)
                  << ", " << std::setw(22) << full_state.ecc
                  << ", " << std::setw(22) << full_state.inc
                  << ", " << std::setw(22) << full_state.argp
			      << ", " << std::setw(22) << full_state.raan
			      << ", " << std::setw(22) << full_state.tran;
    // clang-format on
}

void Output_handler::stream_output_JPL(State_vector out)
{
    output_buffer.precision(13);

    double sec; // Pass this by reference to get fractions of second
    tm UTC_cal = out.epoch.get_UTC_cal(sec);

    sec += UTC_cal.tm_sec; // Add to int tm_sec to get correct value

    // clang-format off
    // Hardcoded for GPS SV46 in JPL format
	output_buffer << 46 << std::setfill('0')
   			<< " " << std::setw(4) << (UTC_cal.tm_year + 1900)
   	  		<< " " << std::setw(2) << (UTC_cal.tm_mon + 1)
   	  		<< " " << std::setw(2) << UTC_cal.tm_mday
   	  		<< " " << std::setw(2) << UTC_cal.tm_hour
   	  		<< " " << std::setw(2) << UTC_cal.tm_min
   	  		<< std::setprecision(2) << std::fixed
			<< " " << std::setw(5) << sec

   	  		<< std::setprecision(6) << std::setfill(' ')
			<< " " << std::setw(13) << out.x
			<< " " << std::setw(13) << out.y
			<< " " << std::setw(13) << out.z
 	 	 	<< std::setprecision(8)
			<< " " << std::setw(11) << out.u
			<< " " << std::setw(11) << out.v
			<< " " << std::setw(11) << out.w;
    // clang-format on
}

void Output_handler::buffer_header(const Resident_space_object &rso)
{
    switch (output_format) {
    case ECI:  // No header
    case ECEF: // No header
    case NONE: // No output at all!
    case STORE:
        break;
    case ALL:
        output_buffer << header_full_state() << ", "
                      << rso.accelerations_header() << ", "
                      << rso.solar_properties_header() << "\n";
        break;
    case ATMOS:
        output_buffer << header_full_state() << ", " 
                      << rso.atmos_properties_header() << "\n";
        break;
    case OPS:
        output_buffer << header_full_state() << "\n";
    }
}

void Output_handler::buffer_output(const Resident_space_object &rso)
{
    // Because Intel compiler doesn't seem to support std::defaultfloat
    // As of verison: icpc (ICC) 17.0.1 20161005
    output_buffer.unsetf(std::ios_base::floatfield);

    switch (output_format) {
    case ECI:
        stream_output_JPL(rso.get_eci());
        output_buffer << "\n";
        break;
    case ECEF:
        stream_output_JPL(rso.get_ecef());
        output_buffer << "\n";
        break;
    case ALL:
        stream_full_state(rso);
        output_buffer << ", ";
        rso.stream_individual_accelerations(output_buffer);
        output_buffer << ", ";
        rso.stream_solar_properties(output_buffer);
        output_buffer << "\n";
        break;
    case NONE: // No output at all!
        break;
    case ATMOS:
        stream_full_state(rso);
        rso.stream_atmos_properties(output_buffer);
        output_buffer << "\n";
        break;
    case STORE:
        store.push_back(std::make_tuple(rso.get_eci(), rso.phiM));
        break;
    case OPS:
        stream_full_state(rso);
        output_buffer << "\n";
    }

    if (output_buffer.tellp() >= buffer_limit) {
        write_output();
    }
}

void Output_handler::write_output()
{
    if (output_buffer.tellp() > 0 && output_format != NONE &&
        output_format != STORE) {
        std::ofstream out_file(output_file,
                               std::ofstream::out | std::ofstream::app);

        if (out_file.good()) {
            out_file << output_buffer.rdbuf(); // Move buffer directly to output
            out_file.close();

            output_buffer.clear(); // Clear any flags
            output_buffer.str(""); // Reset contents of buffer
        } else {
            std::cout << "Error: cannot open output file." << std::endl;
        }
    }
}
