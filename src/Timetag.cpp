/*! @file Timetag.cpp
	@author David Harrison
	@date 30 March 2016
	@brief SGNL OPS Timetag stores time in Terrestrial Time (TAI + 32.184s), in
		   three variables; a Modified Julian Date, the elapsed seconds of the
		   day, and the fraction of the current second.

	This class stores time ONLY as terrestrial time, but it has setters for both
	TT and UTC in various formats including calendar date.
	It also has getters for TT, UTC, TAI, TCG, TCB, TDB and GPS time.

	In converting to and from UTC, leap seconds must be accounted for. At the
    moment this class handles that with leap seconds hard-coded in. It should be
    upgraded to parse a leap second file and automatically populate	the values.

	This class currently ignores the mess of UTC before 1972, but may be
	upgraded to support earlier times if necessary.
 */

#include "../include/Timetag.h"

// Includes all leap seconds from 1st Jan 1972 to 1st Jan 2017.
// Last updated January 2017.
// clang-format off
const long int Timetag::TAI_minus_UTC[num_leaps][2] = {    {41317,10},
           {41499,11}, {41683,12}, {42048,13}, {42413,14}, {42778,15},
           {43144,16}, {43509,17}, {43874,18}, {44239,19}, {44786,20},
           {45151,21}, {45516,22}, {46247,23}, {47161,24}, {47892,25},
           {48257,26}, {48804,27}, {49169,28}, {49534,29}, {50083,30},
           {50630,31}, {51179,32}, {53736,33}, {54832,34}, {56109,35},
           {57204,36}, {57754,37}
};
// clang-format on

const std::vector<long int> Timetag::all_TAI_minus_UTC =
    Timetag::set_TAI_minus_UTC();

std::vector<long int> Timetag::set_TAI_minus_UTC()
{
    const size_t leap_dates = static_cast<size_t>(
        TAI_minus_UTC[num_leaps - 1][0] - TAI_minus_UTC[0][0]);

    std::vector<long int> daily_diff(leap_dates);

    size_t i;
    size_t j = 0;

    for (i = 1; i < num_leaps; ++i) {
        size_t range =
            static_cast<size_t>(TAI_minus_UTC[i][0] - TAI_minus_UTC[0][0]);
        while (j < range) {
            daily_diff[j] = TAI_minus_UTC[i - 1][1];
            ++j;
        }
    }

    return daily_diff;
}

void Timetag::set_timetag_from_tag_TT(Timestruct in)
{
    set_timetag_from_MJD_TT(in.MJDN, in.SOD, in.SOD_frac);
}

void Timetag::set_timetag_from_MJD_TT(long int in_MJDN, double in_SOD)
{
    double SOD_int;

    tag.SOD_frac = std::modf(in_SOD, &SOD_int);
    tag.SOD = static_cast<long int>(SOD_int);

    set_timetag_from_MJD_TT(in_MJDN, tag.SOD, tag.SOD_frac);
}

void Timetag::set_timetag_from_MJD_TT(double in_MJD)
{
    double MJD_int, SOD;

    SOD = std::modf(in_MJD, &MJD_int) * 86400.0;
    tag.MJDN = static_cast<long int>(MJD_int);

    set_timetag_from_MJD_TT(tag.MJDN, SOD);
}

void Timetag::set_timetag_from_JD_TT(double in_JD)
{
    set_timetag_from_MJD_TT(in_JD - JD_minus_MJD);
}

void Timetag::set_timetag_from_cal_TT(long int in_year, long int in_month,
                                      long int in_date, long int in_hour,
                                      long int in_minute, double in_second)
{
    long int sec;

    double sec_int;

    tag.SOD_frac = std::modf(in_second, &sec_int);
    sec = static_cast<long int>(sec_int);

    tag.SOD = (in_hour * 60 + in_minute) * 60 + sec;

    tag.MJDN = calculate_MJDN_from_cal(in_year, in_month, in_date);

    set_timetag_from_MJD_TT(tag.MJDN, tag.SOD, tag.SOD_frac);
}

void Timetag::set_timetag_from_tag_UTC(Timestruct in)
{
    set_timetag_from_MJD_UTC(in.MJDN, in.SOD, in.SOD_frac);
}

void Timetag::set_timetag_from_MJD_UTC(long int in_MJDN_UTC,
                                       long int in_SOD_UTC,
                                       double in_SOD_frac_UTC)
{
    tag.SOD =
        TT_minus_TAI_int + get_TAI_minus_UTC_from_UTC(in_MJDN_UTC) + in_SOD_UTC;
    tag.SOD_frac = TT_minus_TAI_frac + in_SOD_frac_UTC;

    set_timetag_from_MJD_TT(in_MJDN_UTC, tag.SOD, tag.SOD_frac);
}

void Timetag::set_timetag_from_MJD_UTC(long int in_MJDN_UTC, double in_SOD_UTC)
{
    long int SOD_UTC;
    double SOD_int_UTC, SOD_frac_UTC;

    SOD_frac_UTC = std::modf(in_SOD_UTC, &SOD_int_UTC);
    SOD_UTC = static_cast<long int>(SOD_int_UTC);

    set_timetag_from_MJD_UTC(in_MJDN_UTC, SOD_UTC, SOD_frac_UTC);
}

void Timetag::set_timetag_from_MJD_UTC(double in_MJD_UTC)
{
    long int MJDN_UTC;
    double MJD_int_UTC, SOD_UTC;

    SOD_UTC = std::modf(in_MJD_UTC, &MJD_int_UTC) * 86400.0;
    MJDN_UTC = static_cast<long int>(MJD_int_UTC);

    set_timetag_from_MJD_UTC(MJDN_UTC, SOD_UTC);
}

void Timetag::set_timetag_from_JD_UTC(double in_JD_UTC)
{
    set_timetag_from_MJD_UTC(in_JD_UTC - JD_minus_MJD);
}

void Timetag::set_timetag_from_cal_UTC(long int in_year, long int in_month,
                                       long int in_date, long int in_hour,
                                       long int in_minute, double in_second)
{
    long int MJDN_UTC, SOD_UTC, sec;

    double SOD_frac_UTC, sec_int;

    SOD_frac_UTC = std::modf(in_second, &sec_int);
    sec = static_cast<long int>(sec_int);

    SOD_UTC = (in_hour * 60 + in_minute) * 60 + sec;

    MJDN_UTC = calculate_MJDN_from_cal(in_year, in_month, in_date);

    set_timetag_from_MJD_UTC(MJDN_UTC, SOD_UTC, SOD_frac_UTC);
}

void Timetag::set_timetag_from_tag_GPS(Timestruct in)
{
    set_timetag_from_MJD_GPS(in.MJDN, in.SOD, in.SOD_frac);
}

void Timetag::set_timetag_from_MJD_GPS(long int in_MJDN_UTC,
                                       long int in_SOD_UTC,
                                       double in_SOD_frac_UTC)
{
    tag.SOD = TT_minus_TAI_int + TAI_minus_GPS + in_SOD_UTC;
    tag.SOD_frac = TT_minus_TAI_frac + in_SOD_frac_UTC;

    set_timetag_from_MJD_TT(in_MJDN_UTC, tag.SOD, tag.SOD_frac);
}

void Timetag::set_timetag_from_MJD_GPS(long int in_MJDN_GPS, double in_SOD_GPS)
{
    long int SOD_GPS;
    double SOD_int_GPS, SOD_frac_GPS;

    SOD_frac_GPS = std::modf(in_SOD_GPS, &SOD_int_GPS);
    SOD_GPS = static_cast<long int>(SOD_int_GPS);

    set_timetag_from_MJD_GPS(in_MJDN_GPS, SOD_GPS, SOD_frac_GPS);
}

void Timetag::set_timetag_from_MJD_GPS(double in_MJD_GPS)
{
    long int MJDN_GPS;
    double MJD_int_GPS, SOD_GPS;

    SOD_GPS = std::modf(in_MJD_GPS, &MJD_int_GPS) * 86400.0;
    MJDN_GPS = static_cast<long int>(MJD_int_GPS);

    set_timetag_from_MJD_GPS(MJDN_GPS, SOD_GPS);
}

void Timetag::set_timetag_from_JD_GPS(double in_JD_GPS)
{
    set_timetag_from_MJD_GPS(in_JD_GPS - JD_minus_MJD);
}

void Timetag::set_timetag_from_cal_GPS(long int in_year, long int in_month,
                                       long int in_date, long int in_hour,
                                       long int in_minute, double in_second)
{
    long int MJDN_GPS, SOD_GPS, sec;

    double SOD_frac_GPS, sec_int;

    SOD_frac_GPS = std::modf(in_second, &sec_int);
    sec = static_cast<long int>(sec_int);

    SOD_GPS = (in_hour * 60 + in_minute) * 60 + sec;

    MJDN_GPS = calculate_MJDN_from_cal(in_year, in_month, in_date);

    set_timetag_from_MJD_GPS(MJDN_GPS, SOD_GPS, SOD_frac_GPS);
}

// This method adapted from Montenbruck 1st Ed., pg 321, formula A.6
// Valid only from 10th Oct 1582 onwards, but this class enforces a rule
// that no dates may be set before 15th Oct 1582, so the assumption holds.
long int Timetag::calculate_MJDN_from_cal(long int in_year, long int in_month,
                                          long int in_date)
{
    long int n;
    long int y = in_year;
    long int m = in_month;

    // 3 <= m <= 14, moves Jan and Feb to end of the year
    if (m < 3) {
        n = (14 - m) / 12;
        m += n * 12;
        y -= n;
    } else if (m > 14) {
        n = (m - 3) / 12;
        m -= n * 12;
        y += n;
    }

    // We don't need to do date or year validation, we can convert directly to
    // MJD at this point.

    return 365 * y + (153 * (m + 1)) / 5 + in_date + (y / 4) - (y / 100) +
           (y / 400) - 679004;
}

// Fast leap year check:
// http://stackoverflow.com/questions/3220163/how-to-find-leap-year-
// programatically-in-c/11595914#11595914
bool Timetag::is_leap_year(long int y)
{
    return ((y & 3) == 0 && ((y % 25) != 0 || (y & 15) == 0));
}

// Timetag print and string functions
std::string Timetag::str() const
{
    std::stringstream output;
    output << str_MJD() << std::endl << str_datestamp();

    return output.str();
}

std::string Timetag::str_MJD() const
{
    std::stringstream output;

    output << std::setprecision(6) << "MJD: " << std::fixed << get_MJD_TT()
           << std::endl;

    return output.str();
}

std::string Timetag::str_MJD_UTC() const
{
    std::stringstream output;

    output << std::setprecision(6) << "MJD_UTC: " << std::fixed << get_MJD_UTC()
           << std::endl;

    return output.str();
}

std::string Timetag::str_datestamp() const
{
    double sec;
    tm TT_cal = get_TT_cal(sec);

    return str_datestamp(TT_cal, sec) + std::string(" TT");
}

std::string Timetag::str_UTC_datestamp() const
{
    double sec;
    tm UTC_cal = get_UTC_cal(sec);

    return str_datestamp(UTC_cal, sec) + std::string(" UTC");
}

std::string Timetag::str_datestamp(tm cal, double sec)
{
    sec += cal.tm_sec;

    std::stringstream output;

    output << std::setfill(' ') << std::setw(2) << cal.tm_mday << "/"
           << std::setw(2) << (cal.tm_mon + 1) << "/" << (cal.tm_year + 1900)
           << ", " << std::setw(2) << cal.tm_hour << ":" << std::setfill('0')
           << std::setw(2) << cal.tm_min << ":" << std::fixed
           << std::setprecision(6) << std::setw(9) << sec;

    return output.str();
}

void Timetag::print() const
{
    std::cout << str() << std::endl;
}

void Timetag::print_MJD() const
{
    std::cout << str_MJD() << std::endl;
}

void Timetag::print_MJD_UTC() const
{
    std::cout << str_MJD_UTC() << std::endl;
}

void Timetag::print_datestamp() const
{
    std::cout << str_datestamp() << std::endl;
}

void Timetag::print_UTC_datestamp() const
{
    std::cout << str_UTC_datestamp() << std::endl;
}

void Timetag::print_datestamp(tm cal, double sec)
{
    std::cout << str_datestamp(cal, sec) << std::endl;
}

// Returns seconds since TT epoch (midnight, 1st Jan 1977).
double Timetag::get_TT_seconds() const
{
    return static_cast<double>((tag.MJDN - MJD_1_jan_1977) * 86400 + tag.SOD) +
           tag.SOD_frac;
}

long int Timetag::get_MJDN_TT() const
{
    return tag.MJDN;
}

long int Timetag::get_SOD_TT() const
{
    return tag.SOD;
}

double Timetag::get_SOD_frac_TT() const
{
    return tag.SOD_frac;
}

double Timetag::get_MJD_TT() const
{
    return static_cast<double>(tag.MJDN) +
           (static_cast<double>(tag.SOD) + tag.SOD_frac) / 86400.0;
}

double Timetag::get_JD_TT() const
{
    return get_MJD_TT() + JD_minus_MJD;
}

tm Timetag::get_TT_cal() const
{
    return get_tm_from_MJD(tag.MJDN, tag.SOD);
}

// Since the <tm> struct only has seconds as an int, this function can set the
// variable sec_frac (passed by reference) to the decimal part of the seconds.
tm Timetag::get_TT_cal(double &sec_frac) const
{
    sec_frac = tag.SOD_frac;
    return get_TT_cal();
}

// Returns TAI - UTC in seconds
// Only works from 1st Jan 1972 onwards, returns 0 otherwise
long int Timetag::get_TAI_minus_UTC_from_TT() const
{
    long int SOD_TAI = static_cast<long int>(
        floor(static_cast<double>(tag.SOD) + tag.SOD_frac - TT_minus_TAI));

    return get_TAI_minus_UTC_from_TAI(tag.MJDN, SOD_TAI);
}

long int Timetag::get_TAI_minus_UTC_from_TAI(long int in_MJDN_TAI,
                                             long int in_SOD_TAI)
{
    long int offset = get_TAI_minus_UTC_from_UTC(in_MJDN_TAI);

    if (in_SOD_TAI < offset) {
        offset = get_TAI_minus_UTC_from_UTC(in_MJDN_TAI - 1);
    }

    return offset;
}

long int Timetag::get_TAI_minus_UTC_from_UTC(long int in_MJDN_UTC)
{
    const long int start = TAI_minus_UTC[0][0];
    const long int start_leap = TAI_minus_UTC[0][1];

    const long int end = TAI_minus_UTC[num_leaps - 1][0] - 1;
    const long int end_leap = TAI_minus_UTC[num_leaps - 1][1];

    // Before first leap second date
    if (in_MJDN_UTC < start) {
        return start_leap;
    }
    // After last leap second date
    else if (in_MJDN_UTC > end) {
        return end_leap;
    }
    // Somewhere in the middle of the range, so we look it up in the array
    else {
        in_MJDN_UTC -= start;
        return all_TAI_minus_UTC[static_cast<size_t>(in_MJDN_UTC)];
    }
}

long int Timetag::get_seconds_in_UTC_day(long int in_MJDN_UTC)
{
    long int day1_offset, day2_offset;

    day1_offset = get_TAI_minus_UTC_from_UTC(in_MJDN_UTC);
    day2_offset = get_TAI_minus_UTC_from_UTC(in_MJDN_UTC + 1);

    return 86400 + day2_offset - day1_offset;
}

// Returns UTC - UTC in seconds
// Only works from 1st Jan 1972 onwards, returns 0 otherwise
double Timetag::get_TT_minus_UTC() const
{
    return TT_minus_TAI + static_cast<double>(get_TAI_minus_UTC_from_TT());
}

// Returns UTC - UTC in seconds
// Only works from 1st Jan 1972 onwards, returns 0 otherwise
double Timetag::get_TT_minus_UTC(long int MJDN_UTC)
{
    return TT_minus_TAI +
           static_cast<double>(get_TAI_minus_UTC_from_UTC(MJDN_UTC));
}

// Returns UTC - UTC in seconds
// Only works from 1st Jan 1972 onwards, returns 0 otherwise
double Timetag::get_TT_minus_UTC(double MJD_UTC)
{
    return get_TT_minus_UTC(static_cast<long int>(floor(MJD_UTC)));
}

Timestruct Timetag::get_UTC_tag(long int &sec_in_day) const
{
    Timestruct out_tag;

    out_tag.MJDN = tag.MJDN;
    out_tag.SOD = tag.SOD - TT_minus_TAI_int - get_TAI_minus_UTC_from_TT();
    out_tag.SOD_frac = tag.SOD_frac - TT_minus_TAI_frac;

    if (out_tag.SOD_frac < 0.0) {
        out_tag.SOD_frac += 1.0;
        out_tag.SOD--;
    }

    if (out_tag.SOD < 0) {
        out_tag.MJDN--;
        // Could be 86399, 86400 or 86401:
        sec_in_day = get_seconds_in_UTC_day(out_tag.MJDN);
        out_tag.SOD += sec_in_day;
    } else {
        sec_in_day = get_seconds_in_UTC_day(out_tag.MJDN);
    }

    return out_tag;
}

double Timetag::get_MJD_UTC() const
{
    Timestruct UTC;
    long int sec_in_UTC_day;

    UTC = get_UTC_tag(sec_in_UTC_day);

    return static_cast<double>(UTC.MJDN) +
           (static_cast<double>(UTC.SOD) + UTC.SOD_frac) /
               static_cast<double>(sec_in_UTC_day);
}

double Timetag::get_JD_UTC() const
{
    return get_MJD_UTC() + JD_minus_MJD;
}

tm Timetag::get_UTC_cal() const
{
    double dummy;
    return Timetag::get_UTC_cal(dummy);
}

// Since the <tm> struct only has seconds as an int, this function can set the
// variable sec_frac (passed by reference) to the decimal part of the seconds.
tm Timetag::get_UTC_cal(double &sec_frac) const
{
    Timestruct UTC;
    long int sec_in_UTC_day;

    UTC = get_UTC_tag(sec_in_UTC_day);

    sec_frac = UTC.SOD_frac;
    return get_tm_from_MJD(UTC.MJDN, UTC.SOD);
}

Timestruct Timetag::get_TAI_tag() const
{
    Timestruct out_tag;

    out_tag.MJDN = tag.MJDN;
    out_tag.SOD = tag.SOD - TT_minus_TAI_int;
    out_tag.SOD_frac = tag.SOD_frac - TT_minus_TAI_frac;

    if (out_tag.SOD_frac < 0.0) {
        out_tag.SOD_frac += 1.0;
        out_tag.SOD--;
    }

    if (out_tag.SOD < 0) {
        out_tag.SOD += 86400;
        out_tag.MJDN--;
    }

    return out_tag;
}

double Timetag::get_MJD_TAI() const
{
    return static_cast<double>(tag.MJDN) +
           (static_cast<double>(tag.SOD) + tag.SOD_frac - TT_minus_TAI) /
               86400.0;
}

double Timetag::get_JD_TAI() const
{
    return get_MJD_TAI() + JD_minus_MJD;
}

tm Timetag::get_TAI_cal() const
{

    double dummy;
    return Timetag::get_UTC_cal(dummy);
}

// Since the <tm> struct only has seconds as an int, this function can set the
// variable sec_frac (passed by reference) to the decimal part of the seconds.
tm Timetag::get_TAI_cal(double &sec_frac) const
{
    Timestruct TAI = get_TAI_tag();

    sec_frac = TAI.SOD_frac;
    return get_tm_from_MJD(TAI.MJDN, TAI.SOD);
}

// Returns seconds since TCG epoch (midnight, 1st Jan 1977).
// This is an exact relationship, limited by the accuracy of L_G.
double Timetag::get_TCG_seconds() const
{
    return get_TT_seconds() / (1.0 - L_G);
}

double Timetag::get_MJD_TCG() const
{
    return get_TCG_seconds() / 86400.0 + MJD_1_jan_1977;
}

double Timetag::get_JD_TCG() const
{
    return get_MJD_TCG() + JD_minus_MJD;
}

// Returns seconds since TCB epoch (midnight, 1st Jan 1977).
// This relies on an approximate calculation for get_TDB_seconds.
double Timetag::get_TCB_seconds() const
{
    return (get_TDB_seconds() - TDB0) / (1.0 - L_B);
}

double Timetag::get_TCB_seconds(double r_dot_v_E) const
{
    return (get_TDB_seconds(r_dot_v_E) - TDB0) / (1.0 - L_B);
}

double Timetag::get_MJD_TCB() const
{
    return get_TCB_seconds() / 86400.0 + MJD_1_jan_1977;
}

double Timetag::get_MJD_TCB(double r_dot_v_E) const
{
    return get_TCB_seconds(r_dot_v_E) / 86400.0 + MJD_1_jan_1977;
}

double Timetag::get_JD_TCB() const
{
    return get_MJD_TCB() + JD_minus_MJD;
}

double Timetag::get_JD_TCB(double r_dot_v_E) const
{
    return get_MJD_TCB(r_dot_v_E) + JD_minus_MJD;
}

// Quoting from IAU 2006 Resolution B3:
//     The independent time argument of the JPL ephemeris DE405, which is called
//     T_eph (Standish, A&A, 336, 381, 1998), is for practical purposes the same
//     as TDB defined in this Resolution.
//     For solar system ephemerides development the use of TCB is encouraged.
//
// Basically, we should use this as the time input to our ephemeris functions.
//
// This function uses an approximation from Fairhead & Bretagnon (1990) that
// should be accurate to within ~10 microseconds between 1900 - 2100.
// This was based on DE245, so it MAY be less precise than this.
//
// DE430 includes Chebyshev coefficients for TT-TDB accurate to the nanosecond
// and these should be used when we switch to that planetary ephmeris.
double Timetag::get_TDB_minus_TT() const
{
    // Julian years since J2000 in TT:
    double T = ((static_cast<double>(tag.MJDN * 86400 + tag.SOD) -
                 MJD_1_jan_2000_midday * 86400.0) +
                tag.SOD_frac) /
               (86400.0 * 365.25);

    double TDB_minus_TT;

    TDB_minus_TT = 0.102156724E-6 * std::sin(6.283075849991 * T + 4.249032005);

    TDB_minus_TT *= T;

    TDB_minus_TT += 1.115322E-6 * std::sin(3.930209696220 * T + 1.422745069);
    TDB_minus_TT += 1.193379E-6 * std::sin(5.223693919802 * T + 3.649823730);
    TDB_minus_TT += 1.276839E-6 * std::sin(7.860419392439 * T + 5.988822341);
    TDB_minus_TT += 1.554905E-6 * std::sin(77.713772618729 * T + 5.198467090);
    TDB_minus_TT += 1.694205E-6 * std::sin(-0.003523118349 * T + 5.025132748);
    TDB_minus_TT += 2.256707E-6 * std::sin(0.213299095438 * T + 5.543113262);
    TDB_minus_TT += 4.676740E-6 * std::sin(6.069776754553 * T + 4.021195093);
    TDB_minus_TT += 4.770086E-6 * std::sin(0.529690965095 * T + 0.444401603);
    TDB_minus_TT += 13.839792E-6 * std::sin(12.566151886066 * T + 6.196904410);
    TDB_minus_TT += 22.417471E-6 * std::sin(5.753384970095 * T + 4.296977442);
    TDB_minus_TT += 1656.674564E-6 * std::sin(6.283075943033 * T + 6.240054195);

    return TDB_minus_TT;
}

// This method includes an extra relativistic correction term of:
// ( (r_SV - r_E) . V_E ) / ( (1 - L_C) * c^2 )
// Where r_SV is the space craft position and r_E and v_E are the Earth's
// position and velocity, relative to the solar system barycentre.
// This correction can be up to 14 microseconds for a GEO satellite, and up to 3
// microseconds for a LEO satellite.
double Timetag::get_TDB_minus_TT_rel_term(double r_dot_v_E)
{
    return r_dot_v_E * 1000000.0 / ((1.0 - L_C) * sgnlOPS::c2);
}

double Timetag::get_TDB_minus_TT(double r_dot_v_E) const
{
    return get_TDB_minus_TT() + get_TDB_minus_TT_rel_term(r_dot_v_E);
}

double Timetag::get_TDB_seconds() const
{
    return get_TDB_minus_TT() + get_TT_seconds();
}

double Timetag::get_TDB_seconds(double r_dot_v_E) const
{
    return get_TDB_minus_TT(r_dot_v_E) + get_TT_seconds();
}

Timestruct Timetag::get_TDB_tag() const
{
    return get_TDB_tag(0.0);
}

Timestruct Timetag::get_TDB_tag(double r_dot_v_E) const
{
    Timestruct out_tag;
    long int n;

    out_tag.MJDN = tag.MJDN;
    out_tag.SOD = tag.SOD;
    out_tag.SOD_frac = tag.SOD_frac + get_TDB_minus_TT(r_dot_v_E);

    if (out_tag.SOD_frac < 0.0 || out_tag.SOD_frac >= 1.0) {
        n = static_cast<long int>(floor(out_tag.SOD_frac));
        out_tag.SOD_frac -= static_cast<double>(n);
        out_tag.SOD += n;

        if (out_tag.SOD >= 86400) {
            n = out_tag.SOD / 86400;
            out_tag.SOD -= n * 86400;
            out_tag.MJDN += n;
        } else if (out_tag.SOD < 0) {
            n = (86399 - out_tag.SOD) / 86400;
            out_tag.SOD += n * 86400;
            out_tag.MJDN -= n;
        }
    }

    return out_tag;
}

double Timetag::get_MJD_TDB() const
{
    return static_cast<double>(tag.MJDN) +
           (static_cast<double>(tag.SOD) + tag.SOD_frac + get_TDB_minus_TT()) /
               86400.0;
}

double Timetag::get_MJD_TDB(double r_dot_v_E) const
{
    return static_cast<double>(tag.MJDN) +
           (static_cast<double>(tag.SOD) + tag.SOD_frac +
            get_TDB_minus_TT(r_dot_v_E)) /
               86400.0;
}

double Timetag::get_JD_TDB() const
{
    return get_MJD_TDB() + JD_minus_MJD;
}

double Timetag::get_JD_TDB(double r_dot_v_E) const
{
    return get_MJD_TDB(r_dot_v_E) + JD_minus_MJD;
}

Timestruct Timetag::get_GPS_tag() const
{
    Timestruct out_tag;

    out_tag.MJDN = tag.MJDN;
    out_tag.SOD = tag.SOD - TT_minus_TAI_int - TAI_minus_GPS;
    out_tag.SOD_frac = tag.SOD_frac - TT_minus_TAI_frac;

    if (out_tag.SOD_frac < 0.0) {
        out_tag.SOD_frac += 1.0;
        out_tag.SOD--;
    }

    if (out_tag.SOD < 0) {
        out_tag.SOD += 86400;
        out_tag.MJDN--;
    }

    return out_tag;
}

double Timetag::get_MJD_GPS() const
{
    return static_cast<double>(tag.MJDN) +
           (static_cast<double>(tag.SOD - TT_minus_TAI_int - TAI_minus_GPS) +
            (tag.SOD_frac - TT_minus_TAI_frac)) /
               86400.0;
}

double Timetag::get_JD_GPS() const
{
    return get_MJD_GPS() + JD_minus_MJD;
}

void Timetag::get_GPS_format(long int &out_week, long int &out_day,
                             long int &out_hour, long int &out_min,
                             double &out_sec) const
{
    Timestruct GPS_tag;

    GPS_tag = get_GPS_tag();

    if (GPS_tag.MJDN >= MJD_6_jan_1980) {

        ldiv_t GPS_ldiv_t = std::ldiv(GPS_tag.SOD, 3600);

        out_hour = GPS_ldiv_t.quot;

        GPS_ldiv_t = std::ldiv(GPS_ldiv_t.rem, 60);

        out_min = GPS_ldiv_t.quot;
        out_sec = static_cast<double>(GPS_ldiv_t.rem) + GPS_tag.SOD_frac;

        // Max week number is 1023 before it rolls around to 0 again, but we
        // will report the true count to avoid ambiguity and let the function
        // that calls this method deal with it appropriately.
        //
        // The first rollover occured at 23:59:47 UTC on 21st August 1999.
        // The second will be at 23:59:xx UTC on 6th April 2019.
        // Rollover occurs before midnight in UTC due to leap seconds.
        GPS_ldiv_t = std::ldiv((GPS_tag.MJDN - MJD_6_jan_1980), 7);

        out_week = GPS_ldiv_t.quot;
        out_day = GPS_ldiv_t.rem; // GPS days go from 0 - 6
    } else {

        // If Timetag is set before GPS time exists, set everything to zero.
        out_week = 0;
        out_day = 0;
        out_hour = 0;
        out_min = 0;
        out_sec = 0;
    }
}

tm Timetag::get_GPS_cal() const
{
    double dummy;
    return Timetag::get_GPS_cal(dummy);
}

// Since the <tm> struct only has seconds as an int, this function can set the
// variable sec_frac (passed by reference) to the decimal part of the seconds.
tm Timetag::get_GPS_cal(double &sec_frac) const
{
    Timestruct GPS_tag;

    GPS_tag = get_GPS_tag();

    sec_frac = GPS_tag.SOD_frac;
    return get_tm_from_MJD(GPS_tag.MJDN, GPS_tag.SOD);
}

// Doesn't handle fractions of a second because tm_sec is an int.
// This algorithm adapted from Montenbruck, Satellite Orbits, A.1.2, page 322.
//
// This code has been written to take advantage of integer division and bitshift
// operations, while the original algorithm requires many rounding operations.
//
// This code was extensively tested against an independant algorithm by
// Fliegel, H. F., and Van Flandern, T. C.,
// "A Machine Algorithm for Processing Calendar Dates,"
// Communications of the Association of Computing Machines, vol. 11 (1968)
//
// There is a mistake in (at least) the 1st and 3rd Editions of Montenbruck on
// line A.11, it should read "d = [(c-122.1)/365.25]".
tm Timetag::get_tm_from_MJD(long int in_MJDN, long int in_SOD) const
{
    tm output;
    unsigned long int c, d, e, f, g;

    // Convert to Julian day number:
    d = static_cast<unsigned long int>(in_MJDN + 2400001);

    // Find how many 100 year cycles have passed. Typical implementations of
    // this method calculate based on the year 400 AD, but this uses 4801 BC.
    e = ((d << 2) + 128179) / 146097;

    // Fix day number by putting back in all of the "missed" leap days.
    c = d + e - (e >> 2) + 1486;

    // Calculate the year, relative to some offset.
    d = ((c << 3) - 977) / 2922;

    // Calculate day number at start of year.
    e = (1461 * d) >> 2;

    // Calculate the day of the year (starting from day 0 on 1st March).
    g = c - e;

    // Calculate the month (with Jan and Feb moved to the end of the year).
    f = (g << 6) / 1959;

    // Find date, ( 1959 * f ) >> 6 gives elapsed days before current month.
    output.tm_mday = static_cast<int>(g - ((1959 * f) >> 6));

    // Find month minus 1 by moving f into range 0 to 11.
    output.tm_mon = static_cast<int>((f - 2) % 12);

    // Find year minus 1900 by subtracting constant from prior calculation.
    output.tm_year = static_cast<int>(d) - 6615;

    if (in_SOD < 86400) {
        ldiv_t SOD_ldiv_t = std::ldiv(in_SOD, 3600);

        output.tm_hour = static_cast<int>(SOD_ldiv_t.quot);

        SOD_ldiv_t = std::ldiv(SOD_ldiv_t.rem, 60);

        output.tm_min = static_cast<int>(SOD_ldiv_t.quot);
        output.tm_sec = static_cast<int>(SOD_ldiv_t.rem);
    } else {
        output.tm_hour = 23;
        output.tm_min = 59;
        output.tm_sec = static_cast<int>(in_SOD - 86400);
    }

    output.tm_wday = static_cast<int>((in_MJDN + 3) % 7);

    if (output.tm_mon < 2) {
        output.tm_yday = output.tm_mon * 31 + output.tm_mday - 1;
    } else {
        output.tm_year--; // Correction for year when month is Mar to Dec
        output.tm_yday = static_cast<int>(g) - 64;
        if (is_leap_year(1900 + output.tm_year)) {
            ++output.tm_yday;
        }
    }

    output.tm_isdst = 0;

    return output;
}
