/*! \file constants.h
	\brief SGNL OPS header file for defining global constants.
 */

#ifndef SGNL_CONSTANTS_H
#define SGNL_CONSTANTS_H

//! Spacecraft mass--> TO DO: Check that these are still valid
#define IIRmass 1100.0       //!< nominal GPS IIR mass (Kg)
#define IIAa_mass 880.0      //!< nominal GPS IIA mass (Kg) for SVN<23
#define IIAb_mass 972.9      //!< nominal GPS IIA mass (Kg) for SVN>=23
#define GRACE_nom_mass 487.2 //!< kg
#define ICEmass 970.0        //!< nominal ICESat mass (Kg)
#define GFO_nom_mass 369.048 //!< nominal mass (kg), used to create SRP model
#define JASON_nom_mass 489.1 //!< nominal mass (kg), used to create SRP model

/* @author Santosh Bhattarai
   @date 13 March 2015

   Here, the values for constants that are required frequently within the SGNL
   Orbit Prediction Software are defined.
 */
namespace sgnlOPS
{

// Stops compiler warnings for unused variables/parameters
// https://herbsutter.com/2009/10/18/mailbag-shutting-up-compiler-warnings/
template <class T> void ignore(const T &)
{
}

/******************** Physical Constants: ********************/

//! Value of GM used in all gravity models at time of writing, 12 May 2016
//! Recommended by IERS in TN 36, page 79:
//! "398600.4415 should be used by those working with Terrestrial Time (TT)"
static constexpr double GM = 398600.4415; // Units: km^3 / s^2

//! Value of Re used with EGM2008 gravity model
static constexpr double Re = 6378.1363; // Units: km

//! 1 astronomical unit (km), updated on 18th March 2016
//! Source: http://www.iau.org/public/themes/measuring/
//! According to its definition adopted by the XXVIIIth General Asssembly of
//! the IAU (IAU 2012 Resolution B2), the astronomical unit is a
//! conventional unit of length equal to 149 597 870 700 m exactly.
static constexpr double astronomical_unit = 149597870.7;

//! Nominal solar radius defined in IAU 2015 Resolution B3
//! Source: https://www.iau.org/static/resolutions/IAU2015_English.pdf
static constexpr double solar_radius = 695700.0; // Radius of sun in km

//! Speed of light in vacuum (m/s), updated on 18th March 2016
//! Source: http://physics.nist.gov/cgi-bin/cuu/Value?c
static constexpr long int c = 299792458;
static constexpr long double LD_c2 =
    (static_cast<long double>(c)) * (static_cast<long double>(c));
static constexpr double c2 = static_cast<double>(LD_c2);

//! Stefan-Boltzmann constant (Wm^-2K^-4), updated on 18th March 2016
//! Source: http://physics.nist.gov/cgi-bin/cuu/Value?sigma
static constexpr double sigmaSB = 5.670367E-8;

/******************** Mathematical Constants: ********************/

static constexpr long double LD_2_PI = 0x1.921fb54442d1846ap+2L; // 2*pi
static constexpr long double LD_PI = 0x1.921fb54442d1846ap+1L;   // pi
static constexpr long double LD_PI_2 = 0x1.921fb54442d1846ap+0L; // pi/2

static constexpr double D_2_PI = 0x1.921fb54442d18p+2; // 2*pi
static constexpr double D_PI = 0x1.921fb54442d18p+1;   // pi
static constexpr double D_PI_2 = 0x1.921fb54442d18p+0; // pi/2

// Value of pi for double:      0x1.921fb54442d18p+1                16digits
// Value of pi for long double: 0x1.921fb54442d1846ap+1             20digits
// Value of pi for quad:        0x1.921fb54442d18469898cc51701b8p+1 35digits

static constexpr long double LD_DEG2RAD = 0.0174532925199432957692L; // pi/180
static constexpr long double LD_RAD2DEG = 57.295779513082320877L;    // 180/pi

static constexpr double D_DEG2RAD = static_cast<double>(LD_DEG2RAD);
static constexpr double D_RAD2DEG = static_cast<double>(LD_RAD2DEG);

static constexpr double sec_to_rad = static_cast<double>(LD_DEG2RAD / 3600.0L);
static constexpr double microsec_to_rad =
    static_cast<double>(sgnlOPS::LD_DEG2RAD / 3600000000.0L);

/******************** Model Parameters: ********************/

static constexpr long double a = 6378137.0L;
static constexpr long double inv_f = 298257223563.0L;
static constexpr long double LD_a_b = inv_f / (inv_f - 1000000000.0L);
static constexpr long double LD_b_a = (inv_f - 1000000000.0L) / inv_f;
static constexpr long double b = LD_b_a * a;
static constexpr long double LD_e2 =
    (2.0L * inv_f - 1000000000.0L) * 1000000000.0L / (inv_f * inv_f);

// WGS84 reference ellipsoid polar and equatorial radii (km).
// Used in Eclipse_model and Resident_variables classes
static constexpr double wgs84_equatorial_radius =
    static_cast<double>(a / 1000.0L);
static constexpr double wgs84_equatorial_radius2 =
    static_cast<double>(a * a / 1000000.0L);

static constexpr double wgs84_polar_radius = static_cast<double>(b / 1000.0L);
static constexpr double wgs84_polar_radius2 =
    static_cast<double>(b * b / 1000000.0L);

static constexpr double wgs84_inv_flattening =
    static_cast<double>(inv_f / 1000000000.0L);
static constexpr double wgs84_flattening =
    static_cast<double>(1000000000.0L / inv_f);

static constexpr double wgs84_a_b = static_cast<double>(LD_a_b);
static constexpr double wgs84_a_b2 = static_cast<double>(LD_a_b * LD_a_b);
static constexpr double wgs84_b_a = static_cast<double>(LD_b_a);
static constexpr double wgs84_b_a2 = static_cast<double>(LD_b_a * LD_b_a);

static constexpr double wgs84_e2 = static_cast<double>(LD_e2);

// Nominal value for solar irradiance (W/m2) assumed in the creation of grid
// files, this should not be changed. This is used to set a member variable
// in the Force class, which is in turn used in the Force_rp_gridfile class.
static constexpr double nominal_solar_flux = 1368.0;

// A more accurate value for solar irradiance (W/m2), used in Flux_solar class
// (at least until tsi class is finished).
// From MZ MSG2 SRP code 13 March 2015
// static constexpr double solar_flux = 1362.0;

//
// This is the value to use for GPS IIR JOGE paper, submitted August 2017
static constexpr double solar_flux = 1368.0;
//


} //end of namespace sgnlOPS

#endif
