/*! @file Force_rp_gridfile.cpp
	@author David Harrison
	@date 20 May 2016
	@brief SGNL OPS implementation of the UCL gridfile radiation pressure method.
 */

#include "../include/Force_rp_gridfile.h"

void Force_rp_gridfile::setup(const Resident_constants &rso_const,
                              std::shared_ptr<Resident_variables> in_state)
{
    Force_rp::setup(rso_const, in_state);

    double data_denorm = rso_const.nominal_mass /
                         (1000.0 * rso_const.nominal_flux * rso_const.mass);

    std::ifstream x_file, y_file, z_file;

    if (rso_const.nominal_mass <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_rp_gridfile: gridfile nominal mass is 0 or negative, "
              << rso_const.nominal_mass << " kg.";
        state->errors.push_back(error.str());
    }

    if (rso_const.nominal_flux <= 0.0) {
        std::stringstream error;
        error.precision(16);
        error << "Force_rp_gridfile: gridfile nominal flux is 0 or negative, "
              << rso_const.nominal_flux << " W/m^2.";
        state->errors.push_back(error.str());
        data_denorm = 0.0; // Prevent future NaN's
    }

    if (rso_const.mass <= 0.0) {
        // We don't need to warn as Force_rp::setup does this already
        data_denorm = 0.0; // Prevent future NaN's
    }

    if (!verify_all_grid_files(rso_const, x_file, y_file, z_file)) {
        parse_grid_files(data_denorm, x_file, y_file, z_file);
    }
}

void Force_rp_gridfile::compute_acceleration()
{
    // std::chrono::system_clock::time_point t1 = std::chrono::system_clock::now();
    Force_rp::compute_acceleration();
    // std::chrono::system_clock::time_point t2 = std::chrono::system_clock::now();

    a_eci =
        compute_panel_accel(state->eci_fluxes, state->eci_attitude.panel) +
        compute_accel_from_grid_file(state->eci_fluxes, state->eci_attitude);

    a_ecef =
        compute_panel_accel(state->ecef_fluxes, state->ecef_attitude.panel) +
        compute_accel_from_grid_file(state->ecef_fluxes, state->ecef_attitude);

    // std::chrono::system_clock::time_point t3 = std::chrono::system_clock::now();
    // a_ecef =
    //     compute_panel_accel(state->ecef_fluxes, state->ecef_attitude.panel);
    // std::chrono::system_clock::time_point t4 = std::chrono::system_clock::now();
    // a_ecef +=
    //     compute_accel_from_grid_file(state->ecef_fluxes, state->ecef_attitude);
    // std::chrono::system_clock::time_point t5 = std::chrono::system_clock::now();
    //
    // double diff1 =
    //     std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1)
    //         .count();
    // double diff2 =
    //     std::chrono::duration_cast<std::chrono::duration<double>>(t4 - t3)
    //         .count();
    // double diff3 =
    //     std::chrono::duration_cast<std::chrono::duration<double>>(t5 - t4)
    //         .count();
    //
    // double total = diff1 + diff2 + diff3;
    //
    // std::cout << "Time to compute: " << std::fixed << std::setprecision(3)
    //           << (diff1 * 100.0 / total) << " %" << std::endl;
    // std::cout << "Time for panel: " << std::fixed << std::setprecision(3)
    //           << (diff2 * 100.0 / total) << " %" << std::endl;
    // std::cout << "Time for bus: " << std::fixed << std::setprecision(3)
    //           << (diff3 * 100.0 / total) << " %" << std::endl;
    // std::cout << "Number of flux: " << state->ecef_fluxes.size() << std::endl;
    // std::cout << "-----------------------------------------" << std::endl;

    state->total_a_eci += a_eci;
    state->total_a_ecef += a_ecef;
}

/*!
 * @fn compute_accel_from_grid_file(const std::vector<Fluxstruct> &fluxes,
 *                                  const Attitude_state &attitude) const
 * @date 15 May 2016
 * @brief Uses UCL grid file to calculate acceleration from radiation pressure
 */
Cartesian Force_rp_gridfile::compute_accel_from_grid_file(
    const std::vector<Fluxstruct> &fluxes, const Attitude_state &attitude) const
{
    Cartesian a(0.0, 0.0, 0.0);
    Cartesian body_frame;
    double lambda, phi;

    for (const auto &flux : fluxes) {
        // Flux direction in body frame system
        body_frame.x = -dot_product(attitude.x_hat, flux.dir);
        body_frame.y = -dot_product(attitude.y_hat, flux.dir);
        body_frame.z = -dot_product(attitude.z_hat, flux.dir);

        // Longitude of flux in body coordinate frame (deg).
        lambda = sgnlOPS::D_RAD2DEG * std::atan2(body_frame.y, body_frame.x);

        // Latitude of flux in body coordinate frame (deg).
        if (std::abs(body_frame.z) < 1.0) {
            phi = sgnlOPS::D_RAD2DEG * std::asin(body_frame.z);
        } else if (body_frame.z >= 1.0) {
            phi = 90.0;
        } else {
            phi = -90.0;
        }

        // Body frame components of acceleration (km/s^2).
        body_frame = bilinear_interp(lambda, phi) * (flux.mag_sw + flux.mag_lw);

        a += attitude.x_hat * body_frame.x + attitude.y_hat * body_frame.y +
             attitude.z_hat * body_frame.z;
    }

    return a;
}

/*!
 * @fn bilinear_interp(double longitude, double latitude)
 * @date 15 May 2016
 * @brief Interpolates UCL grid file and returns acceleration as Cartesian
 * @param[in] longitude Longitude of incoming flux in the body frame
 * @param[in] latitude  Latitude of incoming flux in the body frame
 */
Cartesian Force_rp_gridfile::bilinear_interp(double longitude,
                                             double latitude) const
{
    Cartesian acc;

    // Raw horizontal and vertical indexed in array (0 indexing).
    double u_raw = longitude - min_longitude;
    double v_raw = latitude - min_latitude;

    double u_floor = std::floor(u_raw);
    double v_floor = std::floor(v_raw);

    double del_u = u_raw - u_floor;
    double del_v = v_raw - v_floor;

    double tol = 0x1.0p-30; // ~10^-9

    size_t i1, i2, j1, j2;

    i1 = static_cast<size_t>(v_floor);
    i2 = i1 + 1;
    j1 = static_cast<size_t>(u_floor);
    j2 = j1 + 1;

    // Perform interpolation based on appropriate neighbors:

    if (del_u > tol) {
        if (del_v > tol) {
            // Interpolation point falls between grid points in both directions:
            Cartesian row1, row2;

            // Horizontal interpolation along upper border.
            row1 = (grid[i1][j2] - grid[i1][j1]) * del_u + grid[i1][j1];
            // Horizontal interpolation along lower border.
            row2 = (grid[i2][j2] - grid[i2][j1]) * del_u + grid[i2][j1];

            // Vertical interpolation (downward) between row1 and row2.
            acc = (row2 - row1) * del_v + row1;
        } else {
            // Interpolation point (pretty much) falls on horizontal grid line:
            // 1D rightward interpolation (increasing longitude).
            acc = (grid[i1][j2] - grid[i1][j1]) * del_u + grid[i1][j1];
        }
    } else {
        if (del_v > tol) {
            // Interpolation point (pretty much) falls on vertical grid line:
            // 1D downward interpolation (increasing latitude).
            acc = (grid[i2][j1] - grid[i1][j1]) * del_v + grid[i1][j1];
        } else {
            // Interpolation point falls on grid node exactly (ish):
            acc = grid[i1][j1];
        }
    }

    return acc;
} // End of bilinear_interp.

bool Force_rp_gridfile::verify_all_grid_files(
    const Resident_constants &rso_const, std::ifstream &x_file,
    std::ifstream &y_file, std::ifstream &z_file)
{
    int error_count = 0;
    std::string x_error, y_error, z_error;

    if (rso_const.x_grid_file == "") {
        x_error = "No grid file provided.";
    } else {
        x_error = verify_single_grid_file("X", rso_const.x_grid_file, x_file);
    }

    if (rso_const.y_grid_file == "") {
        y_error = "No grid file provided.";
    } else {
        y_error = verify_single_grid_file("Y", rso_const.y_grid_file, y_file);
    }

    if (rso_const.z_grid_file == "") {
        z_error = "No grid file provided.";
    } else {
        z_error = verify_single_grid_file("Z", rso_const.z_grid_file, z_file);
    }

    if (x_error != "") {
        error_count++;
    }
    if (y_error != "") {
        error_count++;
    }
    if (z_error != "") {
        error_count++;
    }

    if (error_count > 0) {
        std::string plural = (error_count > 1 ? "s" : "");

        std::stringstream error;
        error << "Force_rp_gridfile: Error" << plural << " reading "
              << error_count << " grid file" << plural << "." << std::endl;

        if (error_count == 1) {
            error << x_error << y_error << z_error;
        } else if (error_count == 3) {
            error << x_error << std::endl << y_error << std::endl << z_error;
        } else {
            if (x_error != "") {
                error << x_error << std::endl << y_error << z_error;
            } else {
                error << y_error << std::endl << z_error;
            }
        }

        state->errors.push_back(error.str());

        return true;
    } else {
        return false; // No errors, success!
    }
}

// Function for verifying the header information in a Surfer6 ascii grid file.
std::string Force_rp_gridfile::verify_single_grid_file(std::string direction,
                                                       std::string file_name,
                                                       std::ifstream &grid_file)
{
    std::vector<std::string> errors;

    grid_file.open(file_name);

    if (!grid_file.good()) {
        std::stringstream error;
        error << "Could not open " << direction << " grid file: " << file_name;
        return error.str();
    }

    size_t n_cols, n_rows; // no. of horizontal and vertical grid nodes.
    int min_lon, max_lon, min_lat, max_lat; // grid file ranges.
    double min_acc, max_acc;

    // First header line:
    std::string scrap;
    getline(grid_file, scrap); // DSAA designation from Surfer6 format.

    // Check grid resolution:
    grid_file >> n_cols;
    if (n_cols != grid_cols) {
        std::stringstream error;
        error << "Expected " << grid_cols << " columns and detected " << n_cols;
        errors.push_back(error.str());
    }

    grid_file >> n_rows;
    if (n_rows != grid_rows) {
        std::stringstream error;
        error << "Expected " << grid_rows << " rows and detected " << n_rows;
        errors.push_back(error.str());
    }

    // Check grid value ranges:
    grid_file >> min_lon;
    if (min_lon != min_longitude) {
        std::stringstream error;
        error << "Expected minimum longitude of " << min_longitude
              << " and detected " << min_lon;
        errors.push_back(error.str());
    }

    grid_file >> max_lon;
    if (max_lon != max_longitude) {
        std::stringstream error;
        error << "Expected maximum longitude of " << max_longitude
              << " and detected " << max_lon;
        errors.push_back(error.str());
    }

    grid_file >> min_lat;
    if (min_lat != min_latitude) {
        std::stringstream error;
        error << "Expected minimum latitude of " << min_latitude
              << " and detected " << min_lat;
        errors.push_back(error.str());
    }

    grid_file >> max_lat;
    if (max_lat != max_latitude) {
        std::stringstream error;
        error << "Expected maximum latitude of " << max_latitude
              << " and detected " << max_lat;
        errors.push_back(error.str());
    }

    grid_file >> min_acc;
    grid_file >> max_acc;

    // std::cout << "Minimum acceleration in " << file_name << " is " << min_acc
    //           << std::endl;
    // std::cout << "Maximum acceleration in " << file_name << " is " << max_acc
    //           << std::endl;

    if (errors.size() > 0u) {
        std::stringstream report;

        report << direction << " grid file error"
               << (errors.size() > 1u ? "s" : "") << ": " << file_name;

        for (const auto &error : errors) {
            report << std::endl << error;
        }

        return report.str();
    } else {
        return "";
    }
} // End of verify_grid_file

// Function for loading a Surfer6 ascii grid file and storing it in memory.
void Force_rp_gridfile::parse_grid_files(double denorm_factor,
                                         std::ifstream &x_file,
                                         std::ifstream &y_file,
                                         std::ifstream &z_file)
{
    // Read in grid node values and populate grid with decreasing row index
    // maintaining the Surfer6 format (upsidedown Mercator).
    for (size_t i = 0; i < grid_rows; ++i) {
        for (size_t j = 0; j < grid_cols; ++j) {
            x_file >> grid[i][j].x;
            y_file >> grid[i][j].y;
            z_file >> grid[i][j].z;
            grid[i][j] *= denorm_factor;
        }
    }
}
