/*! @file kep2car.cpp
 *  @author Santosh Bhattarai
 *  @brief Converts orbit in keplerian elements form to cartesian state vector.
 *  @date 14 January 2015
 */

#include <cstdlib>

#include "../include/Keplerian_elements.h"

int main(int argc, char *argv[])
{

    long double sma, ecc, inc, argp, raan, tran, GM;
    Timetag t;
    t.set_timetag_from_MJD_TT(57468); // MJD for 21 March 2016;

    // Setting passed values or default values for any that were missed.
    // Default GM value is that used by all gravity models in sgnlOPS.
    sma = (argc > 1) ? std::stold(argv[1]) : 6600.0L;
    ecc = (argc > 2) ? std::stold(argv[2]) : 0.0L;
    inc = (argc > 3) ? std::stold(argv[3]) : 0.0L;
    argp = (argc > 4) ? std::stold(argv[4]) : 0.0L;
    raan = (argc > 5) ? std::stold(argv[5]) : 0.0L;
    tran = (argc > 6) ? std::stold(argv[6]) : 0.0L;
    GM = (argc > 7) ? std::stold(argv[7]) : 398600.4415L;

    if (argc == 1) {
        std::cout << std::endl;
        std::cout << "Documentation for kep2car." << std::endl << std::endl;

        std::cout
            << "6 arguments must be specified to fully determine the "
            << "Keplerian elements, in the order; sma, ecc, inc, argp, raan, "
            << "tran. If fewer than 6 are given, default values will be used "
            << "for those not present." << std::endl;
        std::cout
            << "A 7th argument may optionally be passed for the "
            << "gravitational parameter, if it is not then a default is used."
            << std::endl
            << std::endl;

        std::cout << "Here is an example command using the default values."
                  << std::endl;

        std::cout.precision(19);
        std::cout << argv[0] << " " << sma << " " << ecc << " " << inc << " "
                  << argp << " " << raan << " " << tran << " "
                  << std::setprecision(10) << GM << std::endl;

        std::cout << "The program will now run using these vales." << std::endl;

    } else if (argc < 7) {
        std::cout << "*** Warning: Not enough input arguments! ***" << std::endl
                  << "We need a minimum of 6 for the Keplerian elements."
                  << std::endl
                  << "Proceeding anyway using default values for those missed."
                  << std::endl;
    } else if (argc == 7) {
        std::cout.precision(10);
        std::cout
            << "*** Warning: No value for GM specified! ***" << std::endl
            << "Using default value of " << GM << " km^3/s^2." << std::endl
            << "This is the value used by all gravity models in sgnlOPS."
            << std::endl;
    } else if (argc > 8) {
        std::cout
            << "*** Warning: Too many arguments! ***" << std::endl
            << " We only need 6 Keplerian elements and an optional 7th for GM."
            << std::endl
            << "Proceeding anyway using only the first 7 arguments."
            << std::endl;
    }

    Keplerian_elements output(sma, ecc, inc, argp, raan, tran, GM, t);

    std::cout << std::endl << "Input values used:" << std::endl;

    std::cout.precision(19);
    std::cout << "sma = " << output.sma << " km" << std::endl;
    std::cout << "ecc = " << output.ecc << std::endl;
    std::cout << "inc = " << output.inc << " rad" << std::endl;
    std::cout << "argp = " << output.argp << " rad" << std::endl;
    std::cout << "raan = " << output.raan << " rad" << std::endl;
    std::cout << "tran = " << output.tran << " rad" << std::endl;

    std::cout.precision(10);
    std::cout << "GM = " << std::setprecision(10) << output.GM << " km^3/s^2"
              << std::endl;

    std::cout << std::endl << "ECI state vector output:" << std::endl;

    std::cout.precision(19);
    std::cout << "x = " << output.x << " km" << std::endl;
    std::cout << "y = " << output.y << " km" << std::endl;
    std::cout << "z = " << output.z << " km" << std::endl;
    std::cout << "u = " << output.u << " km/s" << std::endl;
    std::cout << "v = " << output.v << " km/s" << std::endl;
    std::cout << "w = " << output.w << " km/s" << std::endl;

    return 0;
}
