/*! @file Two_line_element_reader.cpp
	@author David Harrison
	@date 5 June 2017
	@brief Process CelesTrak JSON data files and store the TLEs as a vector.
 */

#include "../include/Two_line_element_reader.h"

std::vector<Two_line_element> Two_line_element_reader::read_and_sort_TLE(
    const std::vector<std::string> &files, Timetag epoch)
{
    constexpr size_t n_pos = static_cast<size_t>(-1);
    std::vector<Two_line_element> TLEs;

    // Set a reasonable maximum size to prevent reallocation
    TLEs.reserve(files.size() * 20000u);

    for (const auto &file : files) {
        std::ifstream infile(file);
        std::string line;
        std::string contents = "";

        // Stitch the entire file together without line breaks
        while (std::getline(infile, line)) {
            contents += line;
        }

        size_t pos = contents.find("{"); // start of record;
        size_t end = 0;

        while (pos < n_pos) {
            pos = contents.find(",", pos); // skip epoch line
            pos = contents.find(':', pos); // skip line 0 header
            pos = contents.find('"', pos); // start of line 0 field
            pos++;                         // start of line 0 content
            end = contents.find('"', pos); // end of line 0

            std::string line0 = contents.substr(pos, end - pos);

            pos = contents.find(':', end); // skip line 1 header
            pos = contents.find('"', pos); // start of line 1 field
            pos++;                         // start of line 1 content
            end = contents.find('"', pos); // end of line 1

            std::string line1 = contents.substr(pos, end - pos);

            pos = contents.find(':', end); // skip line 2 header
            pos = contents.find('"', pos); // start of line 2 field
            pos++;                         // start of line 2 content
            end = contents.find('"', pos); // end of line 2

            std::string line2 = contents.substr(pos, end - pos);

            pos = contents.find('{', end); // start of next record

            Two_line_element TLE(line0, line1, line2);

            if (TLE.valid_tle()) {
                TLE.set_TLE_time(epoch);

                if (TLE.valid_tle()) {
                    TLEs.push_back(TLE);
                }
            }
        }
    }

    // Sort the TLEs by satellite number, if the same satellites appear
    // multiple times, then sort by closest time to the reference epoch
    std::sort(TLEs.begin(), TLEs.end(), [epoch](const Two_line_element &lhs,
                                                const Two_line_element &rhs) {
        return (lhs.sat_number == rhs.sat_number)
                   ? (std::abs(lhs.epoch_state.epoch - epoch) <
                      std::abs(rhs.epoch_state.epoch - epoch))
                   : (lhs.sat_number < rhs.sat_number);

    });

    std::cout << "TLEs use "
              << static_cast<double>(sizeof(Two_line_element) *
                                     TLEs.capacity()) /
                     (1024.0 * 1024.0)
              << " MB of RAM" << std::endl;

    return TLEs;
}

std::vector<Keplerian_elements> Two_line_element_reader::get_unique_TLE_orbits(
    const std::vector<Two_line_element> &TLEs)
{
    long int last_sat = -1;
    size_t sat_count = 0u;

    // First loop to determine number of satellites
    for (const auto &TLE : TLEs) {
        if (TLE.sat_number != last_sat) {
            last_sat = TLE.sat_number;
            sat_count++;
        }
    }

    // Reserve accordingly
    std::vector<Keplerian_elements> elements;
    elements.reserve(sat_count);

    // Second loop to collect Keplerian elements from TLE data
    for (const auto &TLE : TLEs) {
        if (TLE.sat_number != last_sat) {
            last_sat = TLE.sat_number;
            elements.emplace_back(TLE.current_state, sgnlOPS::GM);
        }
    }

    // Resort the Keplerian elements by semi-major axis
    std::sort(elements.begin(), elements.end(),
              [](const Keplerian_elements &lhs, const Keplerian_elements &rhs) {
                  return (lhs.sma <= rhs.sma);
              });

    std::cout << "Keplerian Elements use "
              << static_cast<double>(sizeof(Keplerian_elements) *
                                     elements.size()) /
                     (1024.0 * 1024.0)
              << " MB of RAM" << std::endl;

    return elements;
}

std::vector<layerstruct> Two_line_element_reader::get_spatial_density(
    const std::vector<Keplerian_elements> &orbits, double altitude_floor,
    double layer_height, size_t num_layers, int orbit_slices)
{
    orbit_slices /= 2;
    double density = 0.5 / static_cast<double>(orbit_slices);

    long double delta_mean =
        sgnlOPS::LD_PI / static_cast<long double>(orbit_slices);

    std::vector<layerstruct> layers(num_layers);
    double altitude = altitude_floor;

    for (auto &layer : layers) {
        layer.altitude = altitude;
        layer.height = layer_height;
        altitude += layer_height;
    }

    std::cout << "Density layers use "
              << static_cast<double>(sizeof(layerstruct) * layers.size()) /
                     (1024.0 * 1024.0)
              << " MB of RAM" << std::endl;

    double altitude_ceiling = altitude;

    int broken = 0;
    int excluded = 0;

    Cartesian pos;
    double r;

    for (auto orbit : orbits) {
        double perigee =
            static_cast<double>((1.0L - orbit.ecc) * orbit.sma) - R;
        double apogee = static_cast<double>((1.0L + orbit.ecc) * orbit.sma) - R;

        if (perigee < altitude_ceiling && apogee > altitude_floor) {

            long double mean = 0.0L;

            orbit.set_mean(mean);

            pos.set(orbit.x, orbit.y, orbit.z);
            r = pos.length();

            if (altitude_floor <= (r - R) && (r - R) <= altitude_ceiling) {
                layers[alt_bin(r, altitude_floor, layer_height)]
                    .d[lat_bin(pos, r)][lon_bin(pos)] += density;
            }

            for (int slice = 1; slice < orbit_slices; slice++) {
                mean += delta_mean;

                orbit.set_mean(mean);

                pos.set(orbit.x, orbit.y, orbit.z);
                r = pos.length();

                if (altitude_floor <= (r - R)) {
                    const size_t alt = alt_bin(r, altitude_floor, layer_height);

                    if (alt < num_layers) {

                        layers[alt].d[lat_bin(pos, r)][lon_bin(pos)] += density;

                        orbit.set_mean(-mean);

                        pos.set(orbit.x, orbit.y, orbit.z);

                        layers[alt].d[lat_bin(pos, r)][lon_bin(pos)] += density;
                    } else {
                        broken++;
                        break;
                    }
                }
            }

            orbit.set_mean(sgnlOPS::LD_PI);

            pos.set(orbit.x, orbit.y, orbit.z);
            r = pos.length();

            if (altitude_floor <= (r - R) && (r - R) < altitude_ceiling) {
                layers[alt_bin(r, altitude_floor, layer_height)]
                    .d[lat_bin(pos, r)][lon_bin(pos)] += density;
            }

        } else {
            excluded++;
        }
    }

    // Divide by spatial volume to get density as objects/km^3
    for (auto &layer : layers) {

        double r1 = 6370 + layer.altitude;
        double r2 = r1 + layer.height;
        double r3 = (r2 * r2 * r2 - r1 * r1 * r1) / 3.0;

        double v1 = r3 * sgnlOPS::D_DEG2RAD;
        double v2 = 0.0;
        double volume = 0.0;

        for (int i = 0; i < 180; ++i) {

            v2 = r3 * sgnlOPS::D_DEG2RAD *
                 std::cos((i + 1) * sgnlOPS::D_DEG2RAD);
            volume = v1 - v2;
            v1 = v2;

            for (int j = 0; j < 360; ++j) {
                layer.d[i][j] /= volume;
            }
        }
    }

    std::cout << broken << " orbits extended beyond altitude limits."
              << std::endl;
    std::cout << excluded
              << " orbits excluded entirely due to altitude constraints."
              << std::endl;

    return layers;
}

size_t Two_line_element_reader::alt_bin(double r, double altitude_floor,
                                        double layer_height)
{
    return static_cast<size_t>(
        std::floor((r - R - altitude_floor) / layer_height));
}

size_t Two_line_element_reader::lat_bin(Cartesian pos, double r)
{
    return static_cast<size_t>(
        std::floor(std::acos(pos.z / r) * sgnlOPS::D_RAD2DEG));
}

size_t Two_line_element_reader::lon_bin(Cartesian pos)
{
    double lon = std::atan2(pos.y, pos.x) * sgnlOPS::D_RAD2DEG;

    if (lon < 0.0) {
        lon += 360.0;
    }

    return static_cast<size_t>(std::floor(lon));
}
