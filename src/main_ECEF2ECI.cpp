/*! @file ECEF2ECI.cpp
 *  @author Stuart Grey
 *  @brief A command line application for converting ECEF to ECI
 *  @date 9 March 2015
 */

#include "../include/Resident_variables.h"

int main(int argc, char *argv[])
{
    long double x, y, z, u, v, w, MJD, sec, sec_int, day_num, day_frac;
    Timestruct UTC;

    MJD = 0.0L;
    sec = 0.0L;
    sec_int = 0.0L;

    x = (argc > 1) ? std::stold(argv[1]) : 6600.0L;
    y = (argc > 2) ? std::stold(argv[2]) : 0.0L;
    z = (argc > 3) ? std::stold(argv[3]) : 0.0L;
    u = (argc > 4) ? std::stold(argv[4]) : 0.0L;
    v = (argc > 5) ? std::stold(argv[5]) : 0.0L;
    w = (argc > 6) ? std::stold(argv[6]) : 0.0L;

    if (argc == 8) {
        MJD = std::stold(argv[7]);
    } else if (argc == 9) {
        UTC.MJDN = std::stol(argv[7]);
        sec = std::stold(argv[8]);
    } else if (argc > 9) {
        UTC.MJDN = std::stol(argv[7]);
        UTC.SOD = std::stol(argv[8]);
        UTC.SOD_frac = std::stod(argv[9]);
    } else {
        MJD = 57468.0L; // MJD for 21 March 2016;
    }

    day_frac = std::modf(MJD, &day_num);
    UTC.MJDN += static_cast<long int>(day_num);

    day_frac *= Timetag::get_seconds_in_UTC_day(UTC.MJDN);

    sec += std::modf(day_frac, &sec_int);
    UTC.SOD += static_cast<long int>(sec_int);

    UTC.SOD_frac += static_cast<double>(std::modf(sec, &sec_int));
    UTC.SOD += static_cast<long int>(sec_int);

    State_vector test_eci, test_ecef;

    test_ecef.x = x; // km
    test_ecef.y = y; // km
    test_ecef.z = z; // km
    test_ecef.u = u; // km/s
    test_ecef.v = v; // km/s
    test_ecef.w = w; // km/s
    test_ecef.epoch.set_timetag_from_tag_UTC(UTC);

    Ephemeris ephem;

    Frame_transform frame_transform;
    frame_transform.setup(test_ecef.epoch, 0);

    test_eci = compute_rotation_to_eci(test_ecef, ephem, frame_transform);

    std::cout.precision(19);

    std::cout << "ECI: " << test_eci.x << " " << test_eci.y << " " << test_eci.z
              << " " << test_eci.u << " " << test_eci.v << " " << test_eci.w
              << " " << std::endl;

    return 0;
}
