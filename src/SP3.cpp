#include "../include/SP3.h"

SP3::SP3()
{
    // do nothing for now
}

void SP3::read_SP3()
{
    int line_counter = 0;

    // Check all requested sp3 files opened successfully
    for (unsigned int i = 0; i < sp3_list.size(); i++) {
        std::ifstream infile(sp3_list[i].c_str());

        if (infile.fail() || !infile.is_open()) {
            printf("File failed to open for reading! Error in sp3read.");
            exit(1);
        }

        // Buffer line
        std::string line;
        Timetag lastEpochRead;

        //-----------------------------------------------------------------
        // Skipping the headerlines
        for (unsigned short j = 0; j < 22; j++) {
            std::getline(infile, line);
            line = "";
            line_counter++;
        }

        // get the first line
        line = "";
        std::getline(infile, line);
        // mark the line we read
        line_counter++;

        // go through all the epochs
        do {
            if (line.substr(0, 3) == std::string("EOF") || line.size() < 2)
                break;

            //-----------------------------------------------------------------
            // Check whether the line is Epoch information or P/V; if Epoch
            //    then parse it
            //    //and read the following line which should be P or V data
            // ELSE ignore this
            //    //statement.
            //    //-----------------------------------------------------------
            if (line[0] == '*') {
                if (line.size() <=
                    30) // line size here should be 31 (characters)
                {
                    printf("SP3::readSP3() -> Invalid line length; It is less "
                           "than 30 char");
                    exit(1);
                }

                int year = std::stoi(line.substr(3, 4));
                int month = std::stoi(line.substr(8, 2));
                int day = std::stoi(line.substr(11, 2));
                int hour = std::stoi(line.substr(14, 2));
                int minute = std::stoi(line.substr(17, 2));
                double second = std::stod(line.substr(20, 11));

                lastEpochRead.set_timetag_from_cal_GPS(year, month, day, hour,
                                                       minute, second);

                line = "";
            } else {
                printf("SP3::readSP3() -> Expected epoch line");
                exit(1);
            }

            // go through all the SV for this epoch
            std::map<std::string, sp3_record> oneEpochRecord;

            do {
                //--------------------------------------------------------------
                // Assign the epoch of this SP3record and parse the line of data
                // ( should not be epoch data )
                //--------------------------------------------------------------

                // We got the Epoch Header record, go ahead and read a SV
                // position or velocity record

                std::getline(infile, line);
                line_counter++;

                if ((toupper(line[0]) == 'P') || (toupper(line[0]) == 'V')) {
                    //--------------------------------------------------------------
                    // Reset the SP3 data structure and read a line from SP3 file.
                    //--------------------------------------------------------------
                    // set the epoch for the SP3Record
                    sp3_record newRecord;
                    newRecord.epoch = lastEpochRead;

                    // SP3Record line size depends on the version of the SP3 file.
                    // Should be at least 60 (characters)
                    if (line.size() < 60) {
                        printf("SP3::readSP3 -> Invalid line length; it is "
                               "less than 60 char ");
                        exit(1);
                    }

                    char gnssCnstl = static_cast<char>(toupper(line[1]));
                    //========= accounts for .APC files from NGS which dont specify Constl
                    //========
                    if (line[1] == ' ') {
                        gnssCnstl = 'G';
                    }
                    // final check
                    if (gnssCnstl == ' ')
                        gnssCnstl = 'G';

                    // create the satellite
                    // sat.setPRN(id);
                    // std::string sat = std::to_string(id);
                    std::string sat = std::string(line.substr(1, 3));
                    // sat = std::itoa()
                    // if (gnssCnstl == 'G')

                    // else if (gnssCnstl == 'E')
                    //     sat.setSystem("E");

                    // set the satellite
                    newRecord.sat_id = sat;
                    // newRecord.satelliteID = sat_gpstk;

                    Cartesian svPos;
                    // if P store the position and if V store the velocity
                    svPos.set(std::stod(line.substr(4, 14)),
                              std::stod(line.substr(18, 14)),
                              std::stod(line.substr(32, 14)));
                    // positional values in km and precise to 1mm
                    // svPos.setY( std::stod(line.substr(18,14)));
                    // svPos.setZ( std::stod(line.substr(32,14)));

                    /// Bad or absent positional and/or velocity values are to reset to
                    /// 0.000000.
                    /// In this case we ignore that record and go for the next record.
                    if (svPos.x == 0.0 || svPos.y == 0.0 ||
                        svPos.z == 0.0) /// X OR Y OR Z is 0.0 value
                    {
                        /// cout << "Recursive is called\n";
                        // printf("SP3::readRecord() -> Invalid SV position: %ld \n",sat);
                        // std::cout << "Invalid position for satellite: " << sat <<
                        // std::endl;
                        /// exit(1);        !!!WARNING:NEED TO SORT OUT HANDLING OF
                        /// INVALID DATA VALUES!!!
                    }

                    // set the position
                    newRecord.sat_position_ecef = svPos;

                    // NOW read the clock info
                    // clock values are in microseconds and precise to 1 ps
                    double clk = std::stod(line.substr(46, 14));
                    // double clkBias = 0;
                    // double clkBias = std::stod(line.substr(70,3));

                    newRecord.clock_offset = clk;
                    // newRecord.clock_bias = clkBias;

                    if (clk >= 999999.0) {
                        // printf("SP3::readRecord() -> Invalid SV clock: %ld \n",sat);
                        // std::cout << " Invalid clock for satellite: " << sat <<
                        // std::endl;
                        newRecord.clock_offset = 0.0;
                        // ERROR HANDLING REQUIRED : FIND SOME WAY TO IGNORE SUCH PRN!
                        // std::exit(1);
                    }

                    // newRecord.clkOffset = clk;
                    // newRecord.clkOffset_Bias = clkBias;
                    oneEpochRecord[sat] = newRecord;
                }

                else {
                    if (line.substr(0, 3) == std::string("EOF") ||
                        line.size() < 2)
                        break;
                    else
                        break;
                }
            } while (true); // end of inner do loop

            // populate the epoch data
            all_sp3_data[lastEpochRead] = oneEpochRecord;
        } while (true);

    } // end of for loop to go through all of the files

    std::cout << " \n Files read successfully! \n";
}

void SP3::get_sp3_time_series()
{
    // Choosing a degree 8 Lagrange interpolating polynomial to determine
    // velocities from position in SP3 files
    constexpr int interp_order = 8; // Should be an even number
    constexpr size_t half_order = static_cast<size_t>(interp_order / 2);

    const size_t num_records = all_sp3_data.size();

    for (auto const &sp3_record : all_sp3_data) {
        for (auto const &sat : sp3_record.second) {
            all_sp3_time_series[sat.first].reserve(num_records);
            all_sp3_time_series[sat.first].push_back(sat.second);
        }
    }

    // Ensure data is sorted chronologically
    for (auto &sat : all_sp3_time_series) {
        std::sort(sat.second.begin(), sat.second.end(),
                  [](const sp3_record &lhs, const sp3_record &rhs) {
                      return (lhs.epoch <= rhs.epoch);
                  });
    }

    // Should be set to true if velocity given
    // To do: find a way to check automatically
    bool velocity_given = false;

    if (!velocity_given) {
        // Loop through all the satellite timeseries to populate velocity
        for (auto &sat : all_sp3_time_series) {

            const size_t num_epochs = sat.second.size();

            // Vector of the time difference in seconds since first epoch
            std::vector<double> t_data;
            t_data.reserve(num_epochs);

            std::vector<Cartesian> r_data;
            r_data.reserve(num_epochs);

            for (const auto &timeseries : sat.second) {
                t_data.push_back(timeseries.epoch - sat.second[0].epoch);
                r_data.push_back(timeseries.sat_position_ecef);
            }

            // Variables to be interpolated
            Cartesian r;
            Cartesian drdt;

            // Loop through a second time to interpolate velocity
            for (size_t i = half_order; i < num_epochs - half_order; ++i) {
                int offset = static_cast<int>(i - half_order);

                auto t_first = t_data.begin() + offset;
                auto t_last = t_first + interp_order + 1;
                std::vector<double> t_range(t_first, t_last);

                auto r_first = r_data.begin() + offset;
                auto r_last = r_first + interp_order + 1;
                std::vector<Cartesian> r_range(r_first, r_last);

                const double t = t_data[i];

                lagrange_interpolation(t_range, r_range, t, r, drdt);

                // Setting the velocity to the interpolated value here
                sat.second[i].sat_velocity_ecef = drdt;
            }
        }
    }
}
