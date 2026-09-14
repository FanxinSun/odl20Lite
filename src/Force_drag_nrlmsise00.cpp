/*! @file Force_drag_nrlmsise00.cpp
	@author UCL SGNL, after John Keeling (2021)
	@date 15 September 2026
	@brief UCL ODL implementation of drag force model using NRLMSISE-00
 */

#include "../include/Force_drag_nrlmsise00.h"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// The reference NRLMSISE-00 release is plain C and carries no C++ guards.
extern "C" {
#include "../external/nrlmsise00/nrlmsise-00.h"
}

void Force_drag_nrlmsise00::setup(const Resident_constants &rso_const,
                                  std::shared_ptr<Resident_variables> in_state)
{
    Force_drag::setup(rso_const, in_state);
}

void Force_drag_nrlmsise00::compute_acceleration()
{
    // Report error, this displays the issue to the user and halts simulation
    if (state->geodetic.alt < 100.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_drag: Altitude too low, " << state->geodetic.alt
              << " km.";
        state->errors.push_back(error.str());
    }

    double rho = get_density(state->eci.epoch.get_MJD_UTC(),
                             state->geodetic.alt, state->geodetic.lat,
                             state->geodetic.lon);
    state->atmos_density = rho;

    // space-craft acceleration in km/s^2 in ECEF frame
    a_ecef = minus500C_dAm * rho * state->ecef_v * state->ecef_rso_vel;
    state->total_a_ecef += a_ecef;
}

double Force_drag_nrlmsise00::get_density(double mjd_utc, double alt,
                                          double lat, double lon)
{
    const long int mjdn = static_cast<long int>(std::floor(mjd_utc));
    const double sec_of_day = (mjd_utc - static_cast<double>(mjdn)) * 86400.0;

    int year = 0;
    int month = 0;
    int day = 0;
    int doy = 0;
    civil_from_mjdn(mjdn, year, month, day, doy);

    const double lat_deg = lat * 180.0 / M_PI;
    const double lon_deg = lon * 180.0 / M_PI;

    const Space_weather sw = weather_for(mjdn);

    struct nrlmsise_flags flags = {};
    // Switch 0 on asks for metres and kilograms, so d[5] comes back as
    // kg/m^3 directly. Switches 1..23 on are the model's standard settings.
    flags.switches[0] = 1;
    for (int i = 1; i < 24; ++i) {
        flags.switches[i] = 1;
    }

    struct nrlmsise_input input = {};
    input.year = year; // currently ignored by the model
    input.doy = doy;
    input.sec = sec_of_day;
    input.alt = alt;
    input.g_lat = lat_deg;
    input.g_long = lon_deg;
    // The model wants UT, local time and longitude to be consistent; this is
    // the relation its own documentation gives.
    input.lst = sec_of_day / 3600.0 + lon_deg / 15.0;
    input.f107A = sw.f107a;
    input.f107 = sw.f107;
    input.ap = sw.ap;
    input.ap_a = NULL;

    struct nrlmsise_output output = {};

    // gtd7d rather than gtd7: its d[5] is the effective total mass density
    // for drag, which includes the anomalous oxygen that matters above 500 km.
    gtd7d(&input, &flags, &output);

    return output.d[5];
}

/*!
 * Reads res/SW-All.csv once. The columns used are DATE, AP_AVG, F10.7_OBS and
 * F10.7_OBS_CENTER81; the observed rather than the adjusted flux is what
 * NRLMSISE-00 asks for, being the flux at the Earth's actual distance from the
 * Sun. Rows the file only predicts carry empty geomagnetic fields, which are
 * left at the defaults.
 */
const std::map<long int, Force_drag_nrlmsise00::Space_weather> &
Force_drag_nrlmsise00::space_weather()
{
    static std::map<long int, Space_weather> table;
    static bool loaded = false;

    if (loaded) {
        return table;
    }
    loaded = true;

    std::ifstream in("../res/SW-All.csv");
    if (!in.is_open()) {
        return table;
    }

    std::string line;
    if (!std::getline(in, line)) { // header
        return table;
    }

    // Resolve the columns by name so a change of layout does not go unnoticed.
    int col_ap = -1;
    int col_f107 = -1;
    int col_f107a = -1;
    {
        std::stringstream header(line);
        std::string name;
        int index = 0;
        while (std::getline(header, name, ',')) {
            if (!name.empty() && name[name.size() - 1] == '\r') {
                name.erase(name.size() - 1);
            }
            if (name == "AP_AVG") {
                col_ap = index;
            } else if (name == "F10.7_OBS") {
                col_f107 = index;
            } else if (name == "F10.7_OBS_CENTER81") {
                col_f107a = index;
            }
            ++index;
        }
    }

    if (col_ap < 0 || col_f107 < 0 || col_f107a < 0) {
        std::cout << "Force_drag_nrlmsise00: res/SW-All.csv does not have the "
                     "expected CelesTrak columns; using default indices."
                  << std::endl;
        return table;
    }

    while (std::getline(in, line)) {
        if (line.size() < 10) {
            continue;
        }

        std::vector<std::string> field;
        std::stringstream row(line);
        std::string cell;
        while (std::getline(row, cell, ',')) {
            field.push_back(cell);
        }

        const int wanted = (col_ap > col_f107a) ? col_ap : col_f107a;
        if (static_cast<int>(field.size()) <= wanted) {
            continue;
        }

        // DATE is the first column, as YYYY-MM-DD.
        const int year = std::atoi(field[0].substr(0, 4).c_str());
        const int month = std::atoi(field[0].substr(5, 2).c_str());
        const int day = std::atoi(field[0].substr(8, 2).c_str());
        if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
            continue;
        }

        Space_weather sw;
        if (!field[col_f107].empty()) {
            sw.f107 = std::atof(field[col_f107].c_str());
        }
        if (!field[col_f107a].empty()) {
            sw.f107a = std::atof(field[col_f107a].c_str());
        }
        if (!field[col_ap].empty()) {
            sw.ap = std::atof(field[col_ap].c_str());
        }
        sw.measured = !field[col_f107].empty();

        table[mjdn_from_civil(year, month, day)] = sw;
    }

    return table;
}

Force_drag_nrlmsise00::Space_weather
Force_drag_nrlmsise00::weather_for(long int mjdn)
{
    const std::map<long int, Space_weather> &table = space_weather();

    Space_weather sw;                 // documented defaults if nothing is found
    std::map<long int, Space_weather>::const_iterator today =
        table.find(mjdn);

    if (today != table.end()) {
        sw = today->second;

        // The model wants the previous day's flux, but the 81-day average and
        // the magnetic index for the day itself.
        std::map<long int, Space_weather>::const_iterator yesterday =
            table.find(mjdn - 1);
        if (yesterday != table.end()) {
            sw.f107 = yesterday->second.f107;
        }
        return sw;
    }

    if (!reported_default_weather) {
        reported_default_weather = true;
        std::cout << "Force_drag_nrlmsise00: no space weather for MJD " << mjdn
                  << " in res/SW-All.csv; using F10.7 = " << sw.f107
                  << ", Ap = " << sw.ap
                  << " for any day the file does not cover."
                  << "\n                       Refresh it with "
                     "scripts/update_spaceweather.sh." << std::endl;
    }
    return sw;
}

/*!
 * Calendar date from an integer MJD, by the usual civil-from-days algorithm.
 * Deriving the date arithmetically avoids parsing a formatted date string,
 * which is fragile across locales and zero-padding.
 */
void Force_drag_nrlmsise00::civil_from_mjdn(long int mjdn, int &year,
                                            int &month, int &day, int &doy)
{
    long int z = mjdn - 40587 + 719468; // shift to a 1st-March-based era
    const long int era = (z >= 0 ? z : z - 146096) / 146097;
    const long int doe = z - era * 146097;
    const long int yoe =
        (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long int y = yoe + era * 400;
    const long int doy_shifted = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long int mp = (5 * doy_shifted + 2) / 153;
    const long int d = doy_shifted - (153 * mp + 2) / 5 + 1;
    const long int m = (mp < 10) ? (mp + 3) : (mp - 9);

    year = static_cast<int>(y + ((m <= 2) ? 1 : 0));
    month = static_cast<int>(m);
    day = static_cast<int>(d);
    doy = static_cast<int>(mjdn - mjdn_from_civil(year, 1, 1)) + 1;
}

long int Force_drag_nrlmsise00::mjdn_from_civil(int year, int month, int day)
{
    long int y = year;
    const long int m = month;
    y -= (m <= 2) ? 1 : 0;
    const long int era = (y >= 0 ? y : y - 399) / 400;
    const long int yoe = y - era * 400;
    const long int mp = (m + 9) % 12;
    const long int doy = (153 * mp + 2) / 5 + day - 1;
    const long int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468 + 40587;
}
