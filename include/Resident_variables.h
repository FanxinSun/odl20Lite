/*! @file Resident_variables.h
	@author Santosh Bhattarai
	@date 10 November 2015
	@brief SGNL OPS header file defining the object Resident_variables, which
           contains and updates all variables needed by the force models.
 */

#ifndef SGNL_RESIDENT_VARIABLES_H
#define SGNL_RESIDENT_VARIABLES_H

#include "Attitude_state.h"
#include "Configuration.h"
#include "Eclipse_model.h"
#include "Ephemeris.h"
#include "Frame_transform.h"

struct Fluxstruct {
    double mag_sw = 0.0; // Mag of SW flux, 0.4 - 4 um approx, for CERES
    double mag_lw = 0.0; // Mag of LW flux, 4 um - 500 um approx, for CERES
    Cartesian dir;       // Unit vector for direction of flux
};

struct LLAstruct {
    double lat = 0.0;  // Latitude (radians)
    double clat = 1.0; // cos(lat)
    double slat = 0.0; // sin(lat)
    double lon = 0.0;  // Longitude (radians)
    double clon = 1.0; // cos(lon)
    double slon = 0.0; // sin(lon)
    double alt = 0.0;  // Altitude above ellipsoid (km)
};

struct Spherical_harmonic_func {
    double V = 0.0;
    double W = 0.0;
};

struct Spherical_harmonic_coef {
    double C = 0.0;
    double S = 0.0;
};

inline State_vector compute_rotation_to_ecef(State_vector eci, Ephemeris &ephem,
                                             Frame_transform &frame_transform)
{
    // Get TDB-TT including relativity correction:
    double TDB_minus_TT = ephem.converge_TDB_TT(eci.get_position(), eci.epoch);

    // Calculate rotation matrices
    polar_motion pm =
        frame_transform.compute_rotations(eci.epoch, TDB_minus_TT);
    sgnlOPS::ignore(pm);

    // Calculate and return ECEF state vector
    return frame_transform.rotate_eci_to_ecef(eci);
}

inline State_vector compute_rotation_to_eci(State_vector ecef, Ephemeris &ephem,
                                            Frame_transform &frame_transform)
{
    // Get TDB-TT without relativity correction:
    double TDB_minus_TT = ecef.epoch.get_TDB_minus_TT();

    // Calculate rotation matrices
    polar_motion pm =
        frame_transform.compute_rotations(ecef.epoch, TDB_minus_TT);

    // First stab at ECI state vector
    State_vector eci = frame_transform.rotate_ecef_to_eci(ecef);

    // Get TDB-TT again, with relativity correction:
    TDB_minus_TT = ephem.converge_TDB_TT(eci.get_position(), ecef.epoch);

    // Calculate updated rotation matrices based on corrected TDB-TT
    pm = frame_transform.compute_rotations(ecef.epoch, TDB_minus_TT);
    sgnlOPS::ignore(pm); // Silence compiler warning

    // Calculate and return ECI state vector based on corrected TDB-TT
    return frame_transform.rotate_ecef_to_eci(ecef);
}

/**
 * @class Resident_variables
 * @date 10 November 2015
 * @author Santosh Bhattarai
 * @brief Resident_variables contains and updates all variables needed by the
 *        force models.
 */
class Resident_variables
{
  public:
    Cartesian total_a_eci;
    Cartesian total_a_ecef;

    size_t degree = 0u; //!< Degree of VW for gravity and Lorentz models
    size_t order = 0u;  //!< Order of VW for gravity and Lorentz models

    State_vector eci;  //!< state vector in Earth Centred Inertial frame
    State_vector ecef; //!< state vector in Earth Centred, Earth Fixed frame

    long double Re_ld = static_cast<long double>(sgnlOPS::Re);
    double Re = sgnlOPS::Re;

    double r2 = 0.0;          //!< magnitude of radius squared
    long double r2_ld = 0.0L; //!< magnitude of radius squared as long double
    double r = 0.0;           //!< magnitude of radius

    double eci_v2 = 0.0; //!< magnitude of velocity squared in ECI frame
    double eci_v = 0.0;  //!< magnitude of velocity in ECI frame

    double eci_r_dot_v = 0.0;

    double ecef_v2 = 0.0; //!< magnitude of velocity squared in ECEF frame
    double ecef_v = 0.0;  //!< magnitude of velocity in ECEF frame

    double panel_flux_sw[2] = {}; // Short-wave flux on solar panels
    double panel_flux_lw[2] = {}; // Long-wave flux on solar panels

    Cartesian eci_r_cross_v;

    Cartesian eci_rso;
    Cartesian eci_rso_hat;

    Cartesian eci_rso_vel;
    // Cartesian eci_rso_vel_hat;

    Cartesian ecef_rso;
    Cartesian ecef_rso_hat;

    Cartesian ecef_rso_vel;
    // Cartesian ecef_rso_vel_hat;

    LLAstruct geodetic;

    Attitude_state eci_attitude;  //!< attitude state
    Attitude_state ecef_attitude; //!< attitude state

    std::vector<Fluxstruct> eci_fluxes;
    std::vector<Fluxstruct> ecef_fluxes;

    bool need_solar_properties = false; //!< ERP, SRP, TRR all need this

    // For V' and W' function coefficients
    std::vector<double> VWh;

    // For V' and W' spherical harmonic functions, used by gravity and mag field
    std::vector<Spherical_harmonic_func> VW;

    /*
	 *	SRP members that are computed in the srp::setup(). These are SRP
	 *	members that are computed from the basic SRP members.
	 */
    Ephemeris ephem; //!< ephemeris is a vector of Cartesian strucure
    Frame_transform frame_transform;
    polar_motion pm;

    Cartesian eci_sun;
    Cartesian eci_sun_hat;
    Cartesian ecef_sun;
    Cartesian ecef_sun_hat;
    double eci_sun_distance = 0.0;

    Cartesian eci_rso_sun;
    Cartesian eci_rso_sun_hat; //!< space-craft sun unit-vector
    Cartesian ecef_rso_sun;
    Cartesian ecef_rso_sun_hat; //!< space-craft sun unit-vector
    double rso_sun_distance = 0.0;
    double rso_sun_distance2 = 0.0;

    double eclipse_state = 1.0;

    //!< the specific relative angular momentum vector, h = R x V
    //!< sometimes the symbol l is also used to refer to this quantity
    Cartesian h;

    //!< the beta angle is the angle between the Sun vector and the
    //!< projection of the sun vector on to the orbital plane
    double beta_angle = 0.0;
    double eps_angle = 0.0;

    //!< atmospheric density should also be stored in the state
    double atmos_density = 0.0;

    Matrix3x3 dadr_eci;
    Matrix3x3 dadr_ecef;

    Matrix3x3 dadv_eci;
    Matrix3x3 dadv_ecef;

    std::vector<std::string> reports; // Information reported from force models
    std::vector<std::string> errors;  // Errors reported from force models

    /*
	 * Member functions
	 */
    Resident_variables() = default;

    void reset();

    void setup(const Configuration &config);
    void enable_solar_properties();

    void update(State_vector in_state);
    void print();

    void set_degree_order(size_t n, size_t m);
    void set_earth_radius(double in_Re);

    // Uses ECEF state vector to evaluate V' and W' at current position
    void populate_VW_prime();

    State_vector compute_rotation_to_ecef(State_vector eci_in)
    {
        return ::compute_rotation_to_ecef(eci_in, ephem, frame_transform);
    }
    State_vector compute_rotation_to_eci(State_vector ecef_in)
    {
        return ::compute_rotation_to_eci(ecef_in, ephem, frame_transform);
    }

  private:
    // Precompute and populate h, used when evaluating V' and W'
    void generate_VW_coefs();

    Cartesian geod2ecef(LLAstruct coord);
    LLAstruct ecef2geod(long double x, long double y, long double z);

    LLAstruct ecef2geod(double x, double y, double z)
    {
        return ecef2geod(static_cast<long double>(x),
                         static_cast<long double>(y),
                         static_cast<long double>(z));
    }

    LLAstruct ecef2geod()
    {
        return ecef2geod(ecef.x, ecef.y, ecef.z);
    }

    LLAstruct ecef2geod(State_vector in_ecef)
    {
        return ecef2geod(in_ecef.x, in_ecef.y, in_ecef.z);
    }

    LLAstruct ecef2geod(Cartesian in_ecef)
    {
        return ecef2geod(in_ecef.x, in_ecef.y, in_ecef.z);
    }
};

#endif
