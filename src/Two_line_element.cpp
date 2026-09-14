/*! @file Two_line_element.cpp
	@author David Harrison
	@date 19 April 2016
	@brief SGNL OPS header file defining the class Two_line_element, which is an
		   class for dealing with two-line elements sets.

	Each instance of a Two_line_element object holds the infromation contained
	in a two-line element set. Two-line elements are important in Astrodynamics
	because they widely used within the community. Orbital elements estimated
	from tracking data for a large number of space resident objects are provided
	to users in the the two-line element format. These two-line element sets
	can be found at www.celestrak.com, where there is also comprehensive
	documentation on the two-line element sets.

	<Hoots80> Models for Propagation of NORAD Element Sets, Spacetrack
		      Report no. 3
	<Vallado13> Fundamental of Astrodynamics and Applications, ed. 4,
		        Section 2.4.2 Two-line element sets
 */

#include "../include/Two_line_element.h"

// Include SGP4 code by Vallardo, see readme PDF for more
#include "../external/sgp4/sgp4ext.cpp"
#include "../external/sgp4/sgp4io.cpp"
#include "../external/sgp4/sgp4unit.cpp"

Two_line_element::Two_line_element()
{
    init_variables();
    set_defaults();
}

Two_line_element::Two_line_element(std::string &in_line1, std::string &in_line2)
{
    init_variables();
    setup(in_line1, in_line2);
}

Two_line_element::Two_line_element(std::string &in_line0, std::string &in_line1,
                                   std::string &in_line2)
{
    init_variables();
    setup(in_line0, in_line1, in_line2);
}

bool Two_line_element::valid_tle() const
{
    return is_valid;
}

std::string Two_line_element::get_name() const
{
    return line0;
}

State_vector Two_line_element::get_current_state() const
{
    return current_state;
}

State_vector Two_line_element::get_epoch_state() const
{
    return epoch_state;
}

bool Two_line_element::set_TLE_time(Timetag in_tag)
{
    // Time in minutes from epoch
    double mfe = (in_tag - epoch_state.epoch) / 60.0;

    double ro[3];
    double vo[3];

    is_valid = sgp4::sgp4(sgp4::gravconst, satrec, mfe, ro, vo);

    if (is_valid) {
        current_state.set(ro[0], ro[1], ro[2], vo[0], vo[1], vo[2], in_tag);
    }

    return is_valid;
}

bool Two_line_element::step_TLE_time(double time_step)
{
    Timetag updated_time = current_state.epoch;

    updated_time.step(time_step);

    return set_TLE_time(updated_time);
}

bool Two_line_element::step_TLE_time(long int time_step_int,
                                     double time_step_frac)
{
    Timetag updated_time = current_state.epoch;

    updated_time.step(time_step_int, time_step_frac);

    return set_TLE_time(updated_time);
}

// The size of these vectors cannot be set in the header, so we do it here.
void Two_line_element::init_variables()
{
    line0 = "";
    line1.resize(17);
    line2.resize(10);
    is_valid = false;
}

// Defaults are for the ISS on 20th April 2016
void Two_line_element::set_defaults()
{
    std::string a, b;

    line0 = "ISS (ZARYA)";
    a = "1 25544U 98067A   16111.18266424  .00006000  00000-0  97222-4 0  9995";
    b = "2 25544  51.6441 347.3562 0001678  49.2608 103.0863 15.54292187995956";

    extract_data(a, b);

    sat_number = 25544;

    // Use TT here incase leap-seconds aren't set yet:
    epoch_state.epoch.set_timetag_from_MJD_TT(57498, 15850, 0.374336);
    current_state = epoch_state;

    mean_motion_1st_deriv = 0.00006;
    mean_motion_2nd_deriv = 0.0;
    b_star = 0.000097222;
    tle_set_number = 999;

    inc = 51.6441;
    raan = 347.3562;
    ecc = 0.0001678;
    argp = 49.2608;
    mean = 103.0863;
    mean_motion = 15.54292187;
    rev_number = 99595;

    // These defaults ARE valid but if they're set, something went wrong.
    is_valid = false;
}

void Two_line_element::setup(std::string &in_line0, std::string &in_line1,
                             std::string &in_line2)
{
    line0 = in_line0;

    setup(in_line1, in_line2);
}

void Two_line_element::setup(std::string &in_line1, std::string &in_line2)
{
    long int line_num_1, line_num_2;
    line_num_1 = 0;
    line_num_2 = 0;

    if (in_line1.length() == 69 && std::isdigit(in_line1.at(0))) {
        line_num_1 = std::stol(in_line1.substr(0, 1));
    }

    if (in_line2.length() == 69 && std::isdigit(in_line2.at(0))) {
        line_num_2 = std::stol(in_line2.substr(0, 1));
    }

    if (line_num_1 != 1 || line_num_2 != 2) {

        // If we don't have both lines there's nothing else that can be done.
        set_defaults();

    } else {

        // No validation is (currently) performed on the extracted data.
        extract_data(in_line1, in_line2);

        long int check_1, check_2;
        long int computed_check_1, computed_check_2;
        long int sat_number_2;

        check_1 = std::stol(line1[16]);
        check_2 = std::stol(line2[9]);

        computed_check_1 = compute_checksum(in_line1);
        computed_check_2 = compute_checksum(in_line2);

        sat_number = std::stol(line1[1]);
        sat_number_2 = std::stol(line2[1]);

        // If this is false, we can still parse the data and let the calling
        // function decide what to do.

        is_valid = (check_1 == computed_check_1 && is_valid &&
                    check_2 == computed_check_2 && sat_number == sat_number_2);

        long int MJDN, SOD;
        long long int sec_in_day, fraction;
        double SOD_frac, power;

        // Line 1

        long int year = std::stol(line1[6]);
        if (year < 57) { // First space launch was Sputnik 1 in 1957
            year += 2000;
        } else {
            year += 1900;
        }

        long int day = std::stol(line1[7]);

        // Timetag class is specifically written to accept "silly" inputs so for
        // instance, 275th Jan is a perfectly acceptable date to pass here.
        MJDN = Timetag::calculate_MJDN_from_cal(year, 1, day);

        sec_in_day = Timetag::get_seconds_in_UTC_day(MJDN);

        fraction = std::stoll(line1[8]) * sec_in_day;

        lldiv_t SOD_lldiv_t = std::lldiv(fraction, 100000000LL);

        SOD = static_cast<long int>(SOD_lldiv_t.quot);
        SOD_frac = static_cast<double>(SOD_lldiv_t.rem);
        SOD_frac /= 100000000.0;

        epoch_state.epoch.set_timetag_from_MJD_UTC(MJDN, SOD, SOD_frac);
        current_state = epoch_state;

        mean_motion_1st_deriv = std::stod(line1[9]) * 2.0;

        power = std::stod(line1[11]) - 5.0;
        mean_motion_2nd_deriv =
            std::stod(line1[10]) * 6.0 * std::pow(10.0, power);

        power = std::stod(line1[13]) - 5.0;
        b_star = std::stod(line1[12]) * std::pow(10.0, power);

        tle_set_number = std::stol(line1[15]);

        // Line 2
        inc = std::stod(line2[2]);
        raan = std::stod(line2[3]);
        ecc = std::stod(line2[4]) / 10000000.0;
        argp = std::stod(line2[5]);
        mean = std::stod(line2[6]);
        mean_motion = std::stod(line2[7]);
        rev_number = std::stol(line2[8]);
    }
}

// From CelesTrak: https://celestrak.com/columns/v04n03/
// To calculate the checksum, simply add the values of all the numbers on each
// line - ignoring all letters, spaces, periods, and plus signs - and assigning
// a value of 1 to all minus signs. The checksum is the last digit of that sum.
long int Two_line_element::compute_checksum(std::string &in_line) const
{
    long int checksum = 0;

    if (in_line.length() != 69) {
        // Already invalid, so return an impossible-to-match checksum
        checksum = -1;
    } else {

        for (size_t i = 0; i < in_line.length() - 1; ++i) {

            if (std::isdigit(in_line.at(i))) {
                checksum += std::stol(in_line.substr(i, 1));
            } else if (in_line.at(i) == '-') {
                checksum++;
            }
        }

        checksum %= 10;
    }

    return checksum;
}

// Runs function by Vallado to setup the satellite record based on TLE data
void Two_line_element::init_sgp4(std::string &in_line1, std::string &in_line2)
{
    // twoline2rv expects lines of 130 chars
    constexpr size_t line_length = 130;
    size_t line1_length, line2_length;

    char longstr1[line_length];
    char longstr2[line_length];

    memset(longstr1, 0, line_length); // Ensure strings are wiped
    memset(longstr2, 0, line_length);

    line1_length = in_line1.length();
    line2_length = in_line2.length();
    // These should be exactly 69 characters long, but we
    // jump through this hoop to ensure memory safety
    line1_length = std::min(line1_length, line_length);
    line2_length = std::min(line2_length, line_length);

    strncpy(longstr1, in_line1.c_str(), line1_length);
    strncpy(longstr2, in_line2.c_str(), line2_length);

    // Populates member variable satrec with all sgp4 satellite info
    sgp4::twoline2rv(longstr1, longstr2, sgp4::gravconst, satrec);

    double ro[3];
    double vo[3];

    is_valid = sgp4::sgp4(sgp4::gravconst, satrec, 0.0, ro, vo);

    // Gets state vector at TLE epoch
    if (is_valid) {
        epoch_state.set(ro[0], ro[1], ro[2], vo[0], vo[1], vo[2]);
    }
}

void Two_line_element::extract_data(std::string &in_line1,
                                    std::string &in_line2)
{
    init_sgp4(in_line1, in_line2);

    // Extracting from line 1:
    line1[0] = in_line1.substr(0, 1); // line number
    // space
    line1[1] = in_line1.substr(2, 5); // satellite number
    line1[2] = in_line1.substr(7, 1); // classification
    // space
    line1[3] = in_line1.substr(9, 2);  // int. desig. (2 digit launch year)
    line1[4] = in_line1.substr(11, 3); // int. desig. (launch number of year)
    line1[5] = in_line1.substr(14, 3); // int. desig. (piece of the launch)
    // space
    line1[6] = in_line1.substr(18, 2); // last 2 digits of epoch year
    line1[7] = in_line1.substr(20, 3); // day of year
    line1[8] = in_line1.substr(24, 8); // fraction of day (after decimal point)
    // space
    line1[9] = in_line1.substr(33, 10); // 1st deriv of mean motion, over 2
    // space
    line1[10] = in_line1.substr(44, 6); // 2nd deriv of mean motion, over 6
    line1[11] = in_line1.substr(50, 2); // exponent for 2nd deriv
    // space
    line1[12] = in_line1.substr(53, 6); // B* drag term
    line1[13] = in_line1.substr(59, 2); // exponent for B* drag term
    // space
    line1[14] = in_line1.substr(62, 1); // ephemeris type
    // space
    line1[15] = in_line1.substr(64, 4); // element set number
    line1[16] = in_line1.substr(68, 1); // checksum digit

    // Extracting from line 2:
    line2[0] = in_line2.substr(0, 1); // line number
    // space
    line2[1] = in_line2.substr(2, 5); // satellite number
    // space
    line2[2] = in_line2.substr(8, 8); // inclination
    // space
    line2[3] = in_line2.substr(17, 8); // right ascension of ascending node
    // space
    line2[4] = in_line2.substr(26, 7); // eccentricity
    // space
    line2[5] = in_line2.substr(34, 8); // argument of perigee
    // space
    line2[6] = in_line2.substr(43, 8); // mean anomaly
    // space
    line2[7] = in_line2.substr(52, 11); // mean motion
    line2[8] = in_line2.substr(63, 5);  // revolution number
    line2[9] = in_line2.substr(68, 1);  // checksum digit
}
