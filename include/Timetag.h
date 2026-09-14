/*! @file Timetag.h
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

// 1961  Jan.  1 - 1961  Aug.  1     1.422 818 0s + (MJD - 37 300) x 0.001 296s
//       Aug.  1 - 1962  Jan.  1     1.372 818 0s +        ""
// 1962  Jan.  1 - 1963  Nov.  1     1.845 858 0s + (MJD - 37 665) x 0.001 123 2s
// 1963  Nov.  1 - 1964  Jan.  1     1.945 858 0s +        ""
// 1964  Jan.  1 -       April 1     3.240 130 0s + (MJD - 38 761) x 0.001 296s
//       April 1 -       Sept. 1     3.340 130 0s +        ""
//       Sept. 1 - 1965  Jan.  1     3.440 130 0s +        ""
// 1965  Jan.  1 -       March 1     3.540 130 0s +        ""
//       March 1 -       Jul.  1     3.640 130 0s +        ""
//       Jul.  1 -       Sept. 1     3.740 130 0s +        ""
//       Sept. 1 - 1966  Jan.  1     3.840 130 0s +        ""
// 1966  Jan.  1 - 1968  Feb.  1     4.313 170 0s + (MJD - 39 126) x 0.002 592s
// 1968  Feb.  1 - 1972  Jan.  1     4.213 170 0s +        ""

// UTC-TAI history: https://hpiers.obspm.fr/iers/bul/bulc/UTC-TAI.history

#ifndef SGNL_TIMETAG_H
#define SGNL_TIMETAG_H

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "constants.h"

struct Timestruct {
    long int MJDN = 0;     // Modified Julian Date Number (integer part of MJD)
    long int SOD = 0;      // Seconds of day
    double SOD_frac = 0.0; // Fraction of seconds
};

class Timetag
{
  private:
    Timestruct tag;

    static constexpr size_t num_leaps = 28;

    static const std::vector<long int> all_TAI_minus_UTC;
    static const long int TAI_minus_UTC[num_leaps][2];

    // This method is private as it attempts NO input validation and instead
    // relies on the method calling it to provide sane inputs.
    // There are methods to provide TT, TAI, UTC and GPS time <tm> structs.
    tm get_tm_from_MJD(long int in_MJDN, long int in_SOD) const;

  public:
    // Irwin & Fukushima, 1999, A numerical time ephemeris of the Earth, A&A
    static constexpr double L_C = 1.48082686741E-8;

    // Defined as such in IAU 2000 Resolution B1.9
    static constexpr double L_G = 6.969290134E-10;

    // Defined as such in IAU 2006 Resolution B3
    static constexpr double L_B = 1.550519768E-8;
    static constexpr double TDB0 = -6.55E-5;

    static constexpr long int TT_minus_TAI_int = 32;
    static constexpr double TT_minus_TAI_frac = 0.184;
    static constexpr double TT_minus_TAI = 32.184;

    static constexpr long int TAI_minus_GPS = 19;

    static constexpr double JD_minus_MJD = 2400000.5;

    // TT, TAI, UTC, TCG & TDB epochs:
    static constexpr long int MJD_1_jan_1977 = 43144;
    // GPS epoch:
    static constexpr long int MJD_6_jan_1980 = 44244;
    // J2000:
    static constexpr double MJD_1_jan_2000_midday = 51544.5;

    // Constructors
    constexpr Timetag()
    {
    }

    static std::vector<long int> set_TAI_minus_UTC();

    // Setters for TT and UTC
    void set_timetag_from_MJD_TT(long int in_MJDN, long int in_SOD,
                                 double in_SOD_frac)
    {
        long int n = 0;

        tag.MJDN = in_MJDN;
        tag.SOD = in_SOD;
        tag.SOD_frac = in_SOD_frac;

        if (tag.SOD_frac < 0.0 || tag.SOD_frac >= 1.0) {
            n = static_cast<long int>(floor(tag.SOD_frac));
            tag.SOD_frac -= static_cast<double>(n);
            tag.SOD += n;
        }

        if (tag.SOD >= 86400) {
            n = tag.SOD / 86400;
            tag.SOD -= n * 86400;
            tag.MJDN += n;
        } else if (tag.SOD < 0) {
            n = (86399 - tag.SOD) / 86400;
            tag.SOD += n * 86400;
            tag.MJDN -= n;
        }
    }

    void set_timetag_from_tag_TT(Timestruct in);
    void set_timetag_from_MJD_TT(long int in_MJDN, double in_SOD);
    void set_timetag_from_MJD_TT(double in_MJD);
    void set_timetag_from_JD_TT(double in_JD);
    void set_timetag_from_cal_TT(long int in_year, long int in_month,
                                 long int in_date, long int in_hour,
                                 long int in_minute, double in_second);

    void set_timetag_from_tag_UTC(Timestruct in);
    void set_timetag_from_MJD_UTC(long int in_MJDN_UTC, long int in_SOD_UTC,
                                  double in_SOD_frac_UTC);
    void set_timetag_from_MJD_UTC(long int in_MJDN_UTC, double in_SOD_UTC);
    void set_timetag_from_MJD_UTC(double in_MJD_UTC);
    void set_timetag_from_JD_UTC(double in_JD_UTC);
    void set_timetag_from_cal_UTC(long int in_year, long int in_month,
                                  long int in_date, long int in_hour,
                                  long int in_minute, double in_second);

    void set_timetag_from_tag_GPS(Timestruct in);
    void set_timetag_from_MJD_GPS(long int in_MJDN_UTC, long int in_SOD_UTC,
                                  double in_SOD_frac_UTC);
    void set_timetag_from_MJD_GPS(long int in_MJDN_UTC, double in_SOD_UTC);
    void set_timetag_from_MJD_GPS(double in_MJD_UTC);
    void set_timetag_from_JD_GPS(double in_JD_UTC);
    void set_timetag_from_cal_GPS(long int in_year, long int in_month,
                                  long int in_date, long int in_hour,
                                  long int in_minute, double in_second);

    // Miscellaneous methods
    static long int calculate_MJDN_from_cal(long int in_year, long int in_month,
                                            long int in_date);

    static bool is_leap_year(long int y);

    // void step(double time_step);
    // void step(long int time_step_sec, double time_step_frac);

    // Steps Timetag by some increment in seconds, can be positive or negative.
    void step(long int time_step_sec, double time_step_frac)
    {
        tag.SOD_frac += time_step_frac;
        tag.SOD += time_step_sec;

        set_timetag_from_MJD_TT(tag.MJDN, tag.SOD, tag.SOD_frac);
    }

    // Steps Timetag by some increment in seconds, can be positive or negative.
    void step(double time_step)
    {
        long int time_step_sec = 0;
        double time_step_sec_int = 0.0;
        double time_step_frac = 0.0;

        time_step_frac = std::modf(time_step, &time_step_sec_int);
        time_step_sec = static_cast<long int>(time_step_sec_int);

        step(time_step_sec, time_step_frac);
    }

    // Steps Timetag by some increment in seconds, can be positive or negative.
    void step(long double time_step)
    {
        long double time_step_sec_int = 0.0L;
        long double time_step_frac = 0.0L;

        time_step_frac = std::modf(time_step, &time_step_sec_int);
        tag.SOD += static_cast<long int>(time_step_sec_int);

        time_step_frac += static_cast<long double>(tag.SOD_frac);

        time_step_frac = std::modf(time_step_frac, &time_step_sec_int);
        tag.SOD += static_cast<long int>(time_step_sec_int);

        tag.SOD_frac = static_cast<double>(time_step_frac);

        set_timetag_from_MJD_TT(tag.MJDN, tag.SOD, tag.SOD_frac);
    }

    // Timetag print and string functions
    std::string str() const;
    std::string str_MJD() const;
    std::string str_MJD_UTC() const;
    std::string str_datestamp() const;
    std::string str_UTC_datestamp() const;
    static std::string str_datestamp(tm cal, double sec);

    void print() const;
    void print_MJD() const;
    void print_MJD_UTC() const;
    void print_datestamp() const;
    void print_UTC_datestamp() const;
    static void print_datestamp(tm cal, double sec);

    // Methods to get properties in TT time
    double get_TT_seconds() const;
    long int get_MJDN_TT() const;
    long int get_SOD_TT() const;
    double get_SOD_frac_TT() const;
    double get_MJD_TT() const;
    double get_JD_TT() const;
    tm get_TT_cal() const;
    tm get_TT_cal(double &sec_frac) const;

    Timestruct get_TT_tag() const
    {
        return tag;
    }

    long int get_TAI_minus_UTC_from_TT() const;
    static long int get_TAI_minus_UTC_from_TAI(long int in_MJDN_TAI,
                                               long int in_SOD_TAI);
    static long int get_TAI_minus_UTC_from_UTC(long int in_MJDN_UTC);
    static long int get_seconds_in_UTC_day(long int in_MJDN_UTC);

    double get_TT_minus_UTC() const;
    static double get_TT_minus_UTC(long int MJDN_UTC);
    static double get_TT_minus_UTC(double MJD_UTC);

    // Methods related to calculation of UTC time
    Timestruct get_UTC_tag(long int &sec_in_day) const;
    double get_MJD_UTC() const;
    double get_JD_UTC() const;
    tm get_UTC_cal() const;
    tm get_UTC_cal(double &sec_frac) const;

    // Methods related to calculation of TAI time
    Timestruct get_TAI_tag() const;
    double get_MJD_TAI() const;
    double get_JD_TAI() const;
    tm get_TAI_cal() const;
    tm get_TAI_cal(double &sec_frac) const;

    // Methods related to calculation of TCG time
    double get_TCG_seconds() const;
    double get_MJD_TCG() const;
    double get_JD_TCG() const;

    // Methods related to calculation of TCB time
    double get_TCB_seconds() const;
    double get_TCB_seconds(double r_dot_v_E) const;
    double get_MJD_TCB() const;
    double get_MJD_TCB(double r_dot_v_E) const;
    double get_JD_TCB() const;
    double get_JD_TCB(double r_dot_v_E) const;

    // Methods related to calculation of TDB time
    double get_TDB_minus_TT() const;
    static double get_TDB_minus_TT_rel_term(double r_dot_v_E);
    double get_TDB_minus_TT(double r_dot_v_E) const;
    double get_TDB_seconds() const;
    double get_TDB_seconds(double r_dot_v_E) const;
    Timestruct get_TDB_tag() const;
    Timestruct get_TDB_tag(double r_dot_v_E) const;
    double get_MJD_TDB() const;
    double get_MJD_TDB(double r_dot_v_E) const;
    double get_JD_TDB() const;
    double get_JD_TDB(double r_dot_v_E) const;

    // Methods related to calculation of GPS time
    double get_GPS_seconds() const;
    Timestruct get_GPS_tag() const;
    double get_MJD_GPS() const;
    double get_JD_GPS() const;
    void get_GPS_format(long int &out_week, long int &out_day,
                        long int &out_hour, long int &out_min,
                        double &out_sec) const;
    tm get_GPS_cal() const;
    tm get_GPS_cal(double &sec_frac) const;

    // Methods related to comparisons between timetags

    inline friend double operator-(const Timetag &T1, const Timetag &T2);

    void operator+=(double delta)
    {
        step(delta);
    }
    void operator+=(long double delta)
    {
        step(delta);
    }

    void operator-=(double delta)
    {
        step(-delta);
    }
    void operator-=(long double delta)
    {
        step(-delta);
    }
};

inline double operator-(const Timetag &T1, const Timetag &T2)
{
    return static_cast<double>((T1.tag.MJDN - T2.tag.MJDN) * 86400 +
                               (T1.tag.SOD - T2.tag.SOD)) +
           (T1.tag.SOD_frac - T2.tag.SOD_frac);
}

inline Timetag operator+(Timetag in, double delta)
{
    in += delta;
    return in;
}
inline Timetag operator+(double delta, Timetag in)
{
    in += delta;
    return in;
}

inline Timetag operator+(Timetag in, long double delta)
{
    in += delta;
    return in;
}
inline Timetag operator+(long double delta, Timetag in)
{
    in += delta;
    return in;
}

inline Timetag operator-(Timetag in, double delta)
{
    in -= delta;
    return in;
}
inline Timetag operator-(Timetag in, long double delta)
{
    in -= delta;
    return in;
}

inline bool operator<(const Timetag &T1, const Timetag &T2)
{
    return ((T1 - T2) < 0.0);
}

inline bool operator<=(const Timetag &T1, const Timetag &T2)
{
    return ((T1 - T2) <= 0.0);
}

inline bool operator>(const Timetag &T1, const Timetag &T2)
{
    return ((T1 - T2) > 0.0);
}

inline bool operator>=(const Timetag &T1, const Timetag &T2)
{
    return ((T1 - T2) >= 0.0);
}

#endif
