/*! @file ECI2ECEF.cpp
 *  @author Stuart Grey
 *  @brief A command line application for converting ECI to ECEF
 *  @date 9 March 2015
 */

#include "../include/Resident_variables.h"

int main(int argc, char *argv[])
{
    long double x, y, z, u, v, w;
    double epoch;

    x = (argc > 1) ? std::stold(argv[1]) : 6600.0L;
    y = (argc > 2) ? std::stold(argv[2]) : 0.0L;
    z = (argc > 3) ? std::stold(argv[3]) : 0.0L;
    u = (argc > 4) ? std::stold(argv[4]) : 0.0L;
    v = (argc > 5) ? std::stold(argv[5]) : 0.0L;
    w = (argc > 6) ? std::stold(argv[6]) : 0.0L;
    epoch = (argc > 7) ? std::stod(argv[7]) : 57468.0; // MJD for 21 March 2016;

    State_vector test_eci, test_ecef;

    test_eci.x = x; // km
    test_eci.y = y; // km
    test_eci.z = z; // km
    test_eci.u = u; // km/s
    test_eci.v = v; // km/s
    test_eci.w = w; // km/s
    test_eci.epoch.set_timetag_from_MJD_UTC(epoch);

    Ephemeris ephem;

    Frame_transform frame_transform;
    frame_transform.setup(test_eci.epoch, 0);

    test_ecef = compute_rotation_to_ecef(test_eci, ephem, frame_transform);

    std::cout.precision(19);

    std::cout << "ECEF: " << test_ecef.x << " " << test_ecef.y << " "
              << test_ecef.z << " " << test_ecef.u << " " << test_ecef.v << " "
              << test_ecef.w << " " << std::endl;

    return 0;
}
