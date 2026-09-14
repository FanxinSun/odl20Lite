/*! @file Configuration.h
	@author Santosh Bhattarai
	@date 13 January 2015
	@brief SGNL OPS header file defining the object Configuration, which parses
           and holds parameters from a config file.

	Each instance of a Configuration object will set the configurable parameters
	relating to the initial conditions (i.e. position, velocity, time, mass),
	the required prediction length, the integrator to be used, as well as all of
	parameters required in the force model.
 */

#ifndef SGNL_CONFIGURATION_H
#define SGNL_CONFIGURATION_H

#include <fstream>
#include <string>

#include "Two_line_element.h"

class Configuration
{
  public:
    std::string t0; //!< initial time, either MJD or YMDhms format

    std::string tle0;
    std::string tle1;
    std::string tle2;
    Two_line_element tle;

    State_vector initial_state;

    // Spacecraft properties:
    std::string spacecraft; //!< spacecraft name or identifier
    double mass;
    //! Cross-sectional area (m^2), reflectivity and specularity used by the
    //! drag and radiation-pressure models. Left at 0 they keep whatever the
    //! spacecraft name sets, or the built-in default.
    double area;
    double reflectivity;
    double specularity;
    double srp_scale;
    //! Sinusoidal modulation of srp_scale, for generating synthetic arcs of
    //! a tumbling object whose projected area varies. Amplitude is a
    //! fraction (0.3 = +/-30%); period is in seconds. Zero amplitude, the
    //! default, leaves the scale constant.
    double srp_scale_amp;
    double srp_scale_period;

    // Simulation settings:
    double simulation_time;
    double output_interval;
    std::string output_format;

    // Propagator settings:
    double step_size;
    int propagator;

    // Earth gravity settings:
    int gravity_model;
    int grav_degree;
    int grav_order;

    // Lorentz force settings:
    double specific_charge;
    int magnetic_model;
    int mag_degree;
    int mag_order;

    // Atmospheric model settings
    int location;
    int GMTtime;

    // These forces have not yet been implemented
    bool pole_tide;        //!< Pole tide correction
    bool solid_earth_tide; //!< Solid Earth tide correction
    bool time_var_grav;    //!< Time variable gravity requires specific gravity
                           //!< models

    bool t0_set;
    bool tle_set;

    // Other force model settings:
    bool antenna_thrust; //!< Antenna thrust
    int drag;           //!< Drag
    bool gr_corrections; //!< General Relativity correction
    int srp;             //!< Solar radiation pressure
    int erp;             //!< Earth radiation pressure
    int trr;             //!< Thermal re-radiation
    int rp_model;        //!< Radiation pressure model
    bool third_body;     //!< Third body gravity
    bool y_bias;         //!< BFS Y-Bias acceleration model

    Configuration(); //!< default constructor
    Configuration(std::string filename);
    //!< Constructor with alternative initial state that overrides config file
    Configuration(std::string filename, std::string x0, std::string y0,
                  std::string z0, std::string u0, std::string v0,
                  std::string w0, std::string in_t0);

    /****** Member functions ******/
    bool parse_config_file(std::string filename);
    void print() const;
    void print_switches() const;

    void set_initial_state();

    void set_defaults();
};

#endif
