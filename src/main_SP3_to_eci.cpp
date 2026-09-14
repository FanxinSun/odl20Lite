/*! @file main_SP3_to_eci.cpp
 *  @author Santosh Bhattarai, reworked for command-line use
 *  @date 15 September 2026
 *  @brief Extract one satellite from SP3 precise orbit files and write its
 *         states in the ECI text format the orbit fitter reads.
 *
 *  Usage:
 *      SP3_to_eci <sat-id> <output.eci> <file.sp3> [more.sp3 ...]
 *
 *  e.g. SP3_to_eci G01 ../output/g01.eci ../res/sp3/IGS0OPSFIN_...SP3
 *
 *  SP3 gives positions in an Earth-fixed frame; velocities are differentiated
 *  from them by the SP3 class using a degree-8 Lagrange polynomial, so the
 *  first and last few epochs of each arc have no usable velocity and are
 *  dropped here. Output is the format JPL_orbit_format reads:
 *
 *      ID  Y M D H M S  X Y Z U V W        (km and km/s, ECI)
 */

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../include/Resident_variables.h"
#include "../include/SP3.h"

int main(int argc, char *argv[])
{
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <sat-id> <output.eci> <file.sp3> [more.sp3 ...]\n"
                  << "  e.g. " << argv[0]
                  << " G01 ../output/g01.eci ../res/sp3/igs.sp3" << std::endl;
        return 1;
    }

    const std::string chosen_sat = argv[1];
    const std::string out_path = argv[2];

    SP3 sp3;
    for (int i = 3; i < argc; ++i) {
        sp3.sp3_list.push_back(argv[i]);
    }

    sp3.read_SP3();
    sp3.get_sp3_time_series();

    auto series = sp3.all_sp3_time_series.find(chosen_sat);
    if (series == sp3.all_sp3_time_series.end()) {
        std::cerr << "No records for satellite " << chosen_sat
                  << " in the given SP3 files. Available:";
        for (const auto &sat : sp3.all_sp3_time_series) {
            std::cerr << " " << sat.first;
        }
        std::cerr << std::endl;
        return 1;
    }

    const std::vector<sp3_record> &records = series->second;

    // The degree-8 differentiation needs four samples either side, so the
    // outermost four epochs carry no velocity.
    const size_t margin = 4;
    if (records.size() <= 2 * margin) {
        std::cerr << "Only " << records.size() << " epochs for " << chosen_sat
                  << "; need more than " << (2 * margin)
                  << " to differentiate velocities." << std::endl;
        return 1;
    }

    std::ofstream outfile(out_path);
    if (!outfile.is_open()) {
        std::cerr << "Could not open " << out_path << " for writing."
                  << std::endl;
        return 1;
    }

    // Frame_transform caches Earth orientation over the interval, so give it
    // the whole span up front.
    Ephemeris ephem;
    Frame_transform frame_transform;
    frame_transform.setup(records[margin].epoch,
                          records[records.size() - margin - 1].epoch);

    // JPL orbit format wants a numeric id; take the digits of e.g. "G01".
    int sat_number = std::atoi(chosen_sat.c_str() + 1);

    outfile << std::fixed;
    size_t written = 0;

    for (size_t i = margin; i + margin < records.size(); ++i) {
        State_vector ecef;
        ecef.x = records[i].sat_position_ecef.x;
        ecef.y = records[i].sat_position_ecef.y;
        ecef.z = records[i].sat_position_ecef.z;
        ecef.u = records[i].sat_velocity_ecef.x;
        ecef.v = records[i].sat_velocity_ecef.y;
        ecef.w = records[i].sat_velocity_ecef.z;
        ecef.epoch = records[i].epoch;

        State_vector eci = compute_rotation_to_eci(ecef, ephem, frame_transform);

        double sec_frac = 0.0;
        tm cal = records[i].epoch.get_UTC_cal(sec_frac);

        outfile << std::setw(3) << sat_number << " " << std::setw(5)
                << (cal.tm_year + 1900) << " " << std::setw(3)
                << (cal.tm_mon + 1) << " " << std::setw(3) << cal.tm_mday << " "
                << std::setw(3) << cal.tm_hour << " " << std::setw(3)
                << cal.tm_min << " " << std::setprecision(6) << std::setw(10)
                << (cal.tm_sec + sec_frac) << " " << std::setprecision(6)
                << std::setw(18) << static_cast<double>(eci.x) << std::setw(18)
                << static_cast<double>(eci.y) << std::setw(18)
                << static_cast<double>(eci.z) << std::setprecision(9)
                << std::setw(18) << static_cast<double>(eci.u) << std::setw(18)
                << static_cast<double>(eci.v) << std::setw(18)
                << static_cast<double>(eci.w) << "\n";
        ++written;
    }

    outfile.close();

    std::cout << "Wrote " << written << " ECI states for " << chosen_sat
              << " to " << out_path << "\n"
              << "  span " << records[margin].epoch.str_UTC_datestamp()
              << " to "
              << records[records.size() - margin - 1].epoch.str_UTC_datestamp()
              << std::endl;

    return 0;
}
