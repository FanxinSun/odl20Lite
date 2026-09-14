/*! @file main_sgnlOPS.cpp
 *  @author Santosh Bhattarai
 *  @brief A command line application for running orbit propagation simulations.
 *  @date 14 January 2015
 *
 *  This is a command line application that accepts various options via
 *  a config file and simulates orbit prediction scenarios using the SGNL
 *  Orbit Prediction Software Version 2017a.
 */

#include "../include/Propagators.h"

int main(int argc, char *argv[])
{
    auto t1 = std::chrono::steady_clock::now();

    // There are two ways to use sgnlOPS:
    // Method 1. Supply two arguments, in which case the propagator will take
    //           initial conditions from the configuration file
    // Method 2. Supply nine arguments, in which case the propagator will use
    //           initial conditions supplied in the command line.
    //           Initial conditions are position (in km) and velocity (in km/s)
    //           and time in UTC (in either MJD or yyyy,mm,dd,hh,ss.ss format)

    if (argc != 3 && argc != 10) {
        std::cout << "Error, incorrect number of arguments supplied. Expected "
                     "either 2 or 9 arguments but found "
                  << (argc - 1) << ".\n"
                  << "The first 2 arguments must be a sgnlOPS config file and"
                     "and desired output file.\n"
                  << "Optionally 7 further arguments may be provided where:\n"
                  << "Arguments 3 - 5 are initial ECI position (in km)\n"
                  << "Arguments 6 - 8 are initial ECI velocity (in km/s)\n"
                  << "Argument 9 is initial time in UTC with format "
                     "YYYY,MM,DD,HH,MM,SS.ssssss or as an MJD."
                  << std::endl;
        return 0;
    }

    std::string config_file(argv[1]);
    std::string outfile(argv[2]);

    std::unique_ptr<Propagators> propagator;

    if (argc == 10) {
        std::string x0(argv[3]);
        std::string y0(argv[4]);
        std::string z0(argv[5]);
        std::string u0(argv[6]);
        std::string v0(argv[7]);
        std::string w0(argv[8]);
        std::string t0(argv[9]);

        const Configuration config(config_file, x0, y0, z0, u0, v0, w0, t0);

        propagator = Propagators::get_prop(config, outfile);
    } else {
        const Configuration config(config_file);

        propagator = Propagators::get_prop(config, outfile);
    }

    if (propagator->rso.is_state_bad()) {
        std::cout << propagator->rso.why_bad_state();
    } else {
        propagator->rso.print_properties();

        propagator->propagate_and_print_time();
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cout << "             Total time: " << std::fixed
              << std::setprecision(6)
              << std::chrono::duration<double>{t2 - t1}.count() << " seconds."
              << std::endl;
    return 0;
}
