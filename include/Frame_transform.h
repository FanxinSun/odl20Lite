/*! @file Frame_transform.h
	@author Santosh Bhattarai
	@date 1 April 2014
	@brief SGNL OPS header file defining functions associated with ecef/eci
	       transformations.

	Functions associated with ecef/eci frame transformations are defined here.
	These include routines that deal with rotations based on Earth rotation
	angle, polar motion, precession and nutation. The inertial frame
	that is used in the SGNL OPS software is J2000.
 */

#ifndef SGNL_FRAME_TRANSFORM_H
#define SGNL_FRAME_TRANSFORM_H

#include <array>
#include <fstream>
#include <random>

#include "Matrix.h"
#include "polynomial.h"

/******************************************************************************
Structure declarations.
******************************************************************************/

struct nutation {
    double Ebar = 0.0;     // mean obliquity of the ecliptic
    double deltaPhi = 0.0; // nutation in longitude
    double E = 0.0;        // mean obliquity + nutation in obliquity
};

struct lunar_solar_arg {
    double MMoon = 0.0; // Mean anomaly of Moon
    double MSun = 0.0;  // Mean anomaly of Sun
    double U = 0.0;     // Argument of longitude of Moon
    double DSun = 0.0;  // Mean elongation of Moon from Sun
    double omega = 0.0; // Longitude of node of Moon
};

struct polar_motion {
    double x = 0.0;
    double y = 0.0;
};

struct IERS_param {
    double w = 0.0;
    double UT1_minus_UTC[5] = {};
    polar_motion pm[5] = {};
    double dPsi[5] = {};
    double dEpsilon[5] = {};
};

struct IERS_record {
    double w = 0.0;
    double UT1_minus_UTC = 0.0;
    polar_motion pm = {};
    double dPsi = 0.0;
    double dEpsilon = 0.0;
};

/**
 * @class Frame_transform
 * @author Santosh Bhattarai
 * @date 10 January 2015
 *
 * @brief This is a class that deals with all parameters associated with the
 *		  transformation from a celestial (i.e. ECI-like) reference frame to
 *		  a terrestrial (i.e. ECEF) frame.
 *
 * <Vallado13> Fundamental of Astrodynamics and Applications, ed. 4,
 *		       Section 3.7 Transforming Celestial and Terrestrial Coordinates
 */
class Frame_transform
{
  public:
    /* The rotation matrices */
    Matrix3x3 R = Matrix3x3::Identity(); //!< ECI/ECEF transformation matrix
    Matrix3x3 V = Matrix3x3::Identity(); //!< velocity transformation matrix

  private:
    /* Need the time period of interest in order to determine rotations */
    long int MJD_startday = 55000l;

    /* Data arrays for holding the Earth Orientation Parameters */
    std::vector<IERS_param> IERS_parameters;

    static const double n_BD[15][2];
    static const double n_AC[78][2];
    static const std::array<uint16_t, 106> n_mul;

    // IERS coefficients to calculate GMST
    static constexpr double GMST0_a = -62.0;           // Multiplied by 10^7
    static constexpr double GMST0_b = 931040.0;        // Multiplied by 10^7
    static constexpr double GMST0_c = 8640184812866.0; // Multiplied by 10^6
    static constexpr double GMST0_d = 24110548410.0;   // Multiplied by 10^6

  public:
    Frame_transform() = default;

    void setup(Timetag start, double sim_time);
    void setup(Timetag start, Timetag end);
    void read_in_data(long int MJD_endday);

    /******************** ECI to ECEF rotation functions ********************/
    Matrix3x3 rotate_eci_to_ecef(Matrix3x3 eci) const
    {
        // According to Montenbruck Satellite Orbits, Page 246
        // dadr(ECEF) = R * dadr(ECI) * inv(R) where R is the ECI to ECEF matrix
        // For Rotation matrix, transpose is the same as inverse
        return R * eci * R.transpose();
    }
    State_vector rotate_eci_to_ecef(State_vector eci) const
    {
        // Warning, R is a matrix, this operation is NOT commutative!
        State_vector ecef = R * eci;

        ecef.u += static_cast<long double>(V(0, 0)) * eci.x +
                  static_cast<long double>(V(0, 1)) * eci.y +
                  static_cast<long double>(V(0, 2)) * eci.z;

        ecef.v += static_cast<long double>(V(1, 0)) * eci.x +
                  static_cast<long double>(V(1, 1)) * eci.y +
                  static_cast<long double>(V(1, 2)) * eci.z;

        ecef.w += static_cast<long double>(V(2, 0)) * eci.x +
                  static_cast<long double>(V(2, 1)) * eci.y +
                  static_cast<long double>(V(2, 2)) * eci.z;

        return ecef;
    }
    Cartesian rotate_eci_to_ecef(Cartesian eci) const
    {
        // Warning, R is a matrix, this operation is NOT commutative!
        return R * eci;
    }

    /******************** ECEF to ECI rotation functions ********************/
    Matrix3x3 rotate_ecef_to_eci(Matrix3x3 ecef) const
    {
        // According to Montenbruck Satellite Orbits, Page 246
        // dadr(ECI) = inv(R) * dadr(ECEF) * R where R is the ECI to ECEF matrix
        // For Rotation matrix, transpose is the same as inverse
        return R.transpose() * ecef * R;
    }
    State_vector rotate_ecef_to_eci(State_vector ecef) const
    {
        // Warning, R is a matrix, this operation is NOT commutative!
        State_vector eci = ecef * R;

        eci.u += ecef.x * static_cast<long double>(V(0, 0)) +
                 ecef.y * static_cast<long double>(V(1, 0)) +
                 ecef.z * static_cast<long double>(V(2, 0));

        eci.v += ecef.x * static_cast<long double>(V(0, 1)) +
                 ecef.y * static_cast<long double>(V(1, 1)) +
                 ecef.z * static_cast<long double>(V(2, 1));

        eci.w += ecef.x * static_cast<long double>(V(0, 2)) +
                 ecef.y * static_cast<long double>(V(1, 2)) +
                 ecef.z * static_cast<long double>(V(2, 2));

        return eci;
    }
    Cartesian rotate_ecef_to_eci(Cartesian ecef) const
    {
        // Warning, R is a matrix, this operation is NOT commutative!
        return ecef * R;
    }

    /**
	 * @fn compute_rotations
	 * @author Santosh Bhattarai
	 * @date 10 February 2015
	 * @brief Populates R and V, the ECEF/ECI transformation matrices.
	 *
	 * This function populates R and V, which are the ECEF/ECI
	 * transformation matrices for position and velocity, respectively.
	 */
    polar_motion compute_rotations(Timetag epoch, double TDB_minus_TT);

  private:
    static std::array<uint16_t, 106> populate_nutation_arrays();

    static lunar_solar_arg calculate_lunar_solar_arguments(double JC_TDB);

    static double RAY(Timestruct UTC, const lunar_solar_arg &ls_arg,
                      IERS_record &record);

    static double compute_GMST0(double du);
    static double compute_GMST(double du, double UT1, double GMST0);

    static double orig_compute_GMST(double du, double UT1);

    static void test_GMST();

    static double compute_GAST(long int MJDN, double GMST, double omega,
                               nutation &nut);

    static nutation compute_nutation(double JC_TDB,
                                     const lunar_solar_arg &ls_arg,
                                     double dEpsilon, double dPsi);

    /**
     * @fn populate_IERS_coefficient_tables
     * @author David Harrison
     * @date 19 June 2016
     * @brief Populate IERS Earth Orientation Parameters from the input file.
     *
     * This function populates the IERS_parameters vector with coefficients to a
     * quadratic to interpolate the IERS Earth Orientation Parameters.
     *
     * The Earth Orientation file to be read is passed in by a string.
     * This is a file of the complete historical IERS Bulletin B
     * EOPs, of which the latest version can be downloaded from:
     * ftp://hpiers.obspm.fr/iers/series/opa/eopc04
     * Using scripts/update_iersb.sh.
     */
    bool populate_IERS_coefficient_tables(std::string eop_file,
                                          long int MJD_endday);

    static void compute_IERS_coefficients(double y1, double y2, double y3,
                                          double y4, double y5, double &a,
                                          double &b, double &c, double &d,
                                          double &e);

    IERS_record interpolate_IERS_parameters(Timestruct UTC,
                                            long int sec_in_day) const;
};

#endif
