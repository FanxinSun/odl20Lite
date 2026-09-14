/*! @file Resident_constants.h
	@author David Harrison
	@date 17 June 2016
	@brief SGNL OPS header file defining the Resident_constants class.
 */

#ifndef SGNL_RESIDENT_CONSTANTS_H
#define SGNL_RESIDENT_CONSTANTS_H

#include "Configuration.h"

/*!
 * @struct Solar_array_struct
 * @date 15 June 2016
 * @author David Harrison
 * @brief Area and reflectivity properties for solar panel surfaces.
 */
struct panel_material {
    double area = 0.0; // Surface area (m^2) for this material
    double refl = 0.0; // Reflectivity coefficient, nu
    double spec = 0.0; // Specularity coefficient, mu
};

/*!
 * @struct Solar_array_struct
 * @date 12 August 2015
 * @author Santosh Bhattarai
 * @brief Variables describing properties of spacecraft solar array
 *
 * These properties could be made member variables of Resident_constants class,
 * but since there are so many of them it might be neater not to.
 */
struct Solar_array_struct {
    std::vector<panel_material> front = {};
    std::vector<panel_material> rear = {};

    double area = 0.0;  // Total surface area of solar array (m^2)
    double power = 0.0; // Commanded power draw (W) on panel

    std::vector<panel_material> yoke = {};

    /*
	  NOTE: By CONVENTION, the 0 element in the following array corresponds
	  to the sunlit side of the solar array
	 */
    double alpha[2] = {};     // Surface absorbtivity
    double epsilon[2] = {};   // Surface emissivity
    double k[2] = {};         // Conductivity (Wm^-1K^-1)
    double thickness[2] = {}; // Panel thickness
    double power_frac = 0.5;  // Fraction of area of panel used to produce power
};

/*!
 * @class Resident_constants
 * @date 17 June 2016
 * @author David Harrison
 * @brief A class to determine and store all constants for a given object.
 */
class Resident_constants
{
  public:
    /*
     * All constants that force models need, with sensible defaults set:
     */
    // Deliberately non-sensible value for mass to catch when this isn't set
    double mass = 0.0;       //!< Mass (kg), used by Drag, SRP, ERP, TRR
    double GM = sgnlOPS::GM; //!< Set in Force_earth_gravity and used by others

    double area = 10.0; //!< Area (m^2), used by Force_drag, Force_rp_analytic
    // Face areas (m^2) of x, y and z faces (in BFS)
    Cartesian face_area = Cartesian(1.0, 1.0, 1.0);
    double antenna_power = 0.0; //!< Power (W), used by Force_antenna_thrust

    double specific_charge = 0.0; //!< Measured in micro-Coulombs / kg

    Solar_array_struct solar_array;

    int gravity_model = 7;  //!< Earth gravity model to use
    int magnetic_model = 0; //!< Earth gravity model to use

    int location = 0; //!< Location to base atmospheric model
    int GMTtime = 0;  //!< Time of day for atmospheric model

    int solar_flux_model = 0;
    int earth_flux_model = 0;

    size_t grav_degree = 20u; //!< Degree of gravity model
    size_t grav_order = 20u;  //!< Order of gravity model

    size_t mag_degree = 0u; //!< Degree of gravity model
    size_t mag_order = 0u;  //!< Order of gravity model

    // Nominal Mass in kg, used with gridfile, mass model
    double nominal_mass = 1000.0;

    // Approximate coefficient of drag for sphere in space
    double drag_coeff = 2.2; //!< Drag coefficient, used by Force_drag

    // Reflectivity and specularity for MLI, approximately
    //! Multiplies the solar radiation pressure acceleration. 1.0 is the
    //! model as configured; the orbit fit estimates it as a 7th parameter,
    //! which absorbs error in the area, reflectivity and mass together.
    double srp_scale = 1.0;

    double nu = 0.65; //!< Reflectivity, used by Force_rp_analytic
    double mu = 0.5;  //!< Specularity, used by Force_rp_analytic

    // For box-and-wing model
    Cartesian nu_pos; // Reflectivity for positive x, y and z faces
    Cartesian nu_neg; // Specularity for positive x, y and z faces
    Cartesian mu_pos; // Reflectivity for negative x, y and z faces
    Cartesian mu_neg; // Specularity for negative x, y and z faces

    // Space-craft specific acceleration in the BFS y-direction
    double y_bias_accel = 0.0;

    // Flux value (W/m^2) assumed by grid file
    double nominal_flux = sgnlOPS::nominal_solar_flux;

    // For now, the GPS IIR grid file will be the default grid file for OPS
    // GPSIIR without NAP antenna
    std::string path = "../analyses/gpsIIR/gpsIIR_gridfiles/";
    std::string x_grid_file = path + "GPSIIRnonapShroudX-R100-Q32W15S0.grd";
    std::string y_grid_file = path + "GPSIIRnonapShroudY-R100-Q50W50S0.grd";
    std::string z_grid_file = path + "GPSIIRnonapShroudZ-R100-Q43W19S0.grd";

    std::string name = "none";

    Resident_constants() = default;

    void print_properties() const;

    void setup(const Configuration &config);

  private:
    void set_name_specific_values(const Configuration &config);
};

#endif
