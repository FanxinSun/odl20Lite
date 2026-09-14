/*! @file Force_rp_gridfile.h
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of the UCL gridfile radiation pressure method.

    Ziebart et al. Computation of Solar Radiation Pressure and Thermal
    Re-radiation models for the GPS IIR spacecraft, Report to the US
    Air force, 2004.
 */

#ifndef SGNL_FORCE_RP_GRIDFILE_H
#define SGNL_FORCE_RP_GRIDFILE_H

#include "Force_rp.h"

/*!
 * @class Force_rp_gridfile
 * @date 20 May 2016
 * @author David Harrison
 * @brief Uses the UCL gridfile method to calculate acceleration due to incoming
 *        flux (in W/m^2).
 */
class Force_rp_gridfile : public Force_rp
{
  private:
    // Some actual constants used for the grid file functions
    static constexpr size_t grid_rows = 181; //!< No. of latitude nodes
    static constexpr size_t grid_cols = 361; //!< No. of longitude nodes
    static constexpr int min_latitude = -90;
    static constexpr int max_latitude = 90;
    static constexpr int min_longitude = -180;
    static constexpr int max_longitude = 180;

    std::array<std::array<Cartesian, grid_cols>, grid_rows> grid = {};

  public:
    Force_rp_gridfile() = default;

    void setup(const Resident_constants &rso_const,
               std::shared_ptr<Resident_variables> in_state) override;

    void compute_acceleration() override;

    Cartesian
    compute_accel_from_grid_file(const std::vector<Fluxstruct> &fluxes,
                                 const Attitude_state &attitude) const;

    Cartesian bilinear_interp(double longitude, double latitude) const;

    bool verify_all_grid_files(const Resident_constants &rso_const,
                               std::ifstream &x_file, std::ifstream &y_file,
                               std::ifstream &z_file);

    std::string verify_single_grid_file(std::string direction,
                                        std::string file_name,
                                        std::ifstream &grid_file);

    void parse_grid_files(double denorm_factor, std::ifstream &x_file,
                          std::ifstream &y_file, std::ifstream &z_file);
};

#endif
