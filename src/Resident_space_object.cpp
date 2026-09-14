/*! @file Resident_space_object.cpp
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS file defining the class Resident_space_object.
 */

#include "../include/Resident_space_object.h"

void Resident_space_object::setup(const Configuration &in_config)
{
    state = std::make_shared<Resident_variables>();

    config = in_config; // Keep a backup

    rso_const.setup(config);
    state->setup(config);

    // Large enough to hold every force model, prevents resizing
    forces.reserve(10);

    // Setup Earth Gravity first as other forces use model-dependant GM value
    auto temp_earth_grav = std::make_unique<Force_earth_gravity>();
    temp_earth_grav->setup(rso_const, state);

    // Set this constant, based on gravity model, for other force models to use
    rso_const.GM = temp_earth_grav->get_GM();

    if (config.antenna_thrust) {
        forces.push_back(std::make_unique<Force_antenna_thrust>());
    }
    if (config.gr_corrections) {
        forces.push_back(std::make_unique<Force_gr_correction>());
    }
    if (config.magnetic_model > 0) {
        forces.push_back(std::make_unique<Force_lorentz>());
    }
    if (config.y_bias > 0) {
        forces.push_back(std::make_unique<Force_y_bias>());
    }
    if (config.drag > 0) {
        forces.push_back(Force_drag::get_drag_model(config.drag));
    }

    if (config.rp_model > 0) {
        forces.push_back(Force_rp::get_rp_model(config.rp_model));

        // WARNING: Make sure TRR comes after radiation pressure class
        if (config.trr == 1) {
            forces.push_back(std::make_unique<Force_trr>());
        } else if (config.trr == 2) {
            forces.push_back(std::make_unique<Force_trr2>());
        } else if (config.trr == 3) {
            forces.push_back(std::make_unique<Force_trr3>());
        }
    }

    if (config.third_body) {
        forces.push_back(std::make_unique<Force_third_body>());
    }

    if (config.solid_earth_tide) {
        forces.push_back(std::make_unique<Force_earth_tide>());
    }

    for (auto &force : forces) {
        force->setup(rso_const, state);
    }

    initial_state = Keplerian_elements(config.initial_state,
                                       static_cast<long double>(rso_const.GM));

    // auto t1 = std::chrono::steady_clock::now();
    // temp_earth_grav = gravity_statistics(std::move(temp_earth_grav));
    // auto t2 = std::chrono::steady_clock::now();
    //
    // std::cout << "     Gravity Statistics: " << std::fixed
    //           << std::setprecision(6)
    //           << std::chrono::duration<double>{t2 - t1}.count() << " seconds."
    //           << std::endl;

    // Put Earth Gravity at the end so its acceleration is summed last
    forces.push_back(std::move(temp_earth_grav));

    if (config.rp_model > 0) {
        // Needed as Force_rp outputs acceleration in both eci and ecef frames
        acc.resize(forces.size() + 1u);
    } else {
        acc.resize(forces.size());
    }

    if (config.tle_set) {
        sgp4_state = config.tle;
    }

    // if (config.drag > 0)
    // {
    //     // we want to set int method = config.drag
    // }

    update_with_acc_and_deriv(config.initial_state);
}

std::unique_ptr<Force_earth_gravity> Resident_space_object::gravity_statistics(
    std::unique_ptr<Force_earth_gravity> grav)
{
    State_vector test_rso = initial_state;
    test_rso.x = 0.0L;
    test_rso.y = 0.0L;
    test_rso.z = 0.0L;
    test_rso.u = 0.0L;
    test_rso.v = 0.0L;
    test_rso.w = 0.0L;

    long double r = state->Re_ld;

    constexpr size_t points = 1000000u;
    constexpr size_t n_lim = 360u; // Config file used must use at least this
    constexpr size_t m_lim = n_lim;

    double max_an[n_lim + 1] = {};
    double max_an_r[n_lim + 1] = {};
    double max_an_sigma[n_lim + 1] = {};
    double max_anm[n_lim + 1][m_lim + 1] = {};
    double an_rms[n_lim + 1] = {};

    long double phi = 0.0L;
    long double h = 0.0L;

    for (size_t p = 0; p < points; ++p) {

        // Use spiral points algorithm to evenly sample sphere
        h = (2.0L * p / (points - 1.0L)) - 1.0L;

        if (p != 0 && p != points - 1) {
            phi += 3.6L / std::sqrt(points * (1.0L - h * h));
        } else {
            phi = 0.0L;
        }

        test_rso.x = r * std::sqrt(1.0L - h * h);
        test_rso.y = test_rso.x * std::sin(phi);
        test_rso.x *= std::cos(phi);
        test_rso.z = r * h;

        Cartesian rso_norm(test_rso.x, test_rso.y, test_rso.z, r);

        // Insert test position into Resident_variables
        state->ecef = test_rso;
        state->r2_ld = state->ecef.x * state->ecef.x +
                       state->ecef.y * state->ecef.y +
                       state->ecef.z * state->ecef.z;
        state->r2 = static_cast<double>(state->r2_ld);
        state->r = static_cast<double>(std::sqrt(state->r2_ld));

        state->populate_VW_prime();

        // Grab accelerations for each degree/order of interest and get stats
        for (size_t n = 0; n <= n_lim; ++n) {

            Cartesian an, anm;
            double an_mag = 0.0;
            double anm_mag = 0.0;
            double an_r = 0.0;
            double an_sigma = 0.0;

            for (size_t m = 0; m <= n; ++m) {
                anm = grav->get_a(n, m);
                an += anm;
                anm_mag = anm.length();
                if (anm_mag > max_anm[n][m]) {
                    max_anm[n][m] = anm_mag;
                }
            }

            an_mag = an.length2();

            an_r = std::abs(dot_product(an, rso_norm));
            an_sigma = an_mag - an_r * an_r;
            an_sigma = (an_sigma < 0.0) ? 0.0 : std::sqrt(an_sigma);

            an_rms[n] += an_mag;
            an_mag = std::sqrt(an_mag);
            if (an_mag > max_an[n]) {
                max_an[n] = an_mag;
            }
            if (an_r > max_an_r[n]) {
                max_an_r[n] = an_r;
            }
            if (an_sigma > max_an_sigma[n]) {
                max_an_sigma[n] = an_sigma;
            }
        }
    }

    std::cout << std::setprecision(16);
    std::cout << "var Re = " << state->Re_ld << ";" << std::endl;
    std::cout << "var max_a = [];" << std::endl;
    for (size_t n = 0; n <= n_lim; ++n) {
        an_rms[n] = std::sqrt(an_rms[n] / points);

        std::cout << "max_a[" << n << "] = " << max_an[n] << ";" << std::endl;
        // std::cout << "max_a_r[" << n << "] = " << max_an_r[n] << ";"
        //           << std::endl;
        // std::cout << "max_a_sigma[" << n << "] = " << max_an_sigma[n] << ";"
        //           << std::endl;
        // std::cout << n << "_rms: " << an_rms[n] << std::endl;
    }

    std::cout << std::endl << std::endl << "max_a_r" << std::endl;
    for (size_t n = 0; n <= n_lim; ++n) {
        std::cout << max_an_r[n] << std::endl;
    }

    std::cout << std::endl << std::endl << "max_a_sigma" << std::endl;
    for (size_t n = 0; n <= n_lim; ++n) {
        std::cout << max_an_sigma[n] << std::endl;
    }

    return grav;
}

void Resident_space_object::compute_partial_derivatives()
{
    for (auto &force : forces) {
        force->compute_partial_derivatives();
    }
}

Matrix6x6 Resident_space_object::get_partial_derivatives() const
{
    Matrix3x3 dadr =
        state->dadr_eci +
        state->frame_transform.rotate_ecef_to_eci(state->dadr_ecef);

    Matrix3x3 dadv =
        state->dadv_eci +
        state->frame_transform.rotate_ecef_to_eci(state->dadv_ecef);

    Matrix6x6 dfdy;
    dfdy << Matrix3x3::Zero(), Matrix3x3::Identity(), dadr, dadv;

    // Testing code to compare computed dadr matrix to that of monopole gravity
    // Matrix3x3 dadr2, diff;
    // double GM = 398600.4415;
    //
    // double x2 = state->eci.x * state->eci.x;
    // double y2 = state->eci.y * state->eci.y;
    // double z2 = state->eci.z * state->eci.z;
    // double xy = state->eci.x * state->eci.y;
    // double xz = state->eci.x * state->eci.z;
    // double yz = state->eci.y * state->eci.z;
    // double r2 = state->r2;
    //
    // dadr2 << (3.0*x2-r2), 3.0*xy, 3.0*xz,
    //          3.0*xy, (3.0*y2-r2), 3.0*yz,
    //          3.0*xz, 3.0*yz, (3.0*z2-r2);
    //
    // dadr2 *= GM / (r2 * r2 * state->r);
    //
    // diff = dadr2 - dadr;
    //
    // std::cout << dadr << std::endl << std::endl;
    // std::cout << dadr2 << std::endl << std::endl;
    // std::cout << diff << std::endl << std::endl;
    // std::cout << "---------------------------" << std::endl;

    // std::cout << dfdy << std::endl;
    // std::cout << "--------------------" << std::endl;

    return dfdy;
}

Matrix6x6 Resident_space_object::compute_and_get_partial_derivatives()
{
    compute_partial_derivatives();

    return get_partial_derivatives();
}

void Resident_space_object::compute_acceleration()
{
    for (auto &force : forces) {
        force->compute_acceleration();
    }
}

Cartesian Resident_space_object::get_acceleration() const
{
    return state->total_a_eci +
           state->frame_transform.rotate_ecef_to_eci(state->total_a_ecef);
}

Cartesian Resident_space_object::compute_and_get_acceleration()
{
    compute_acceleration();

    return get_acceleration();
}

std::string Resident_space_object::accelerations_header() const
{
    // clang-format off
    return "       Antenna X      " ", "
           "       Antenna Y      " ", "
           "       Antenna Z      " ", "
           "     Antenna Total    " ", "
           "        Drag X        " ", "
           "        Drag Y        " ", "
           "        Drag Z        " ", "
           "      Drag Total      " ", "
           "    GR Correction X   " ", "
           "    GR Correction Y   " ", "
           "    GR Correction Z   " ", "
           "  GR Correction Total " ", "
           "       Lorentz X      " ", "
           "       Lorentz Y      " ", "
           "       Lorentz Z      " ", "
           "     Lorentz Total    " ", "
           "   ECI Rad. Pres. X   " ", "
           "   ECI Rad. Pres. Y   " ", "
           "   ECI Rad. Pres. Z   " ", "
           "  ECI Rad. Pres. Total" ", "
           "   ECEF Rad. Pres. X  " ", "
           "   ECEF Rad. Pres. Y  " ", "
           "   ECEF Rad. Pres. Z  " ", "
           " ECEF Rad. Pres. Total" ", "
           "         TRR X        " ", "
           "         TRR Y        " ", "
           "         TRR Z        " ", "
           "       TRR Total      " ", "
           "   Third Body Grav X  " ", "
           "   Third Body Grav Y  " ", "
           "   Third Body Grav Z  " ", "
           " Third Body Grav Total" ", "
           "    Earth Gravity X   " ", "
           "    Earth Gravity Y   " ", "
           "    Earth Gravity Z   " ", "
           "  Earth Gravity Total ";
    // clang-format on
}

void Resident_space_object::stream_individual_accelerations(
    std::stringstream &output_buffer) const
{
    Cartesian a_antenna_thrust, a_gr_corrections, a_lorentz, a_drag, a_rp_eci,
        a_rp_ecef, a_trr, a_third_body, a_earth_gravity;

    size_t i = 0;

    if (config.antenna_thrust) {
        a_antenna_thrust = forces[i]->get_acceleration_in_eci();
        i++;
    }
    if (config.gr_corrections) {
        a_gr_corrections = forces[i]->get_acceleration_in_eci();
        i++;
    }
    if (config.magnetic_model > 0) {
        a_lorentz = forces[i]->get_acceleration_in_eci();
        i++;
    }
    if (config.drag) {
        a_drag = forces[i]->get_acceleration_in_eci();
        // double density = forces[i]->ussa1976_density();
        // std::cout << "Density: " << forces[i]->g << std::endl;
        i++;
    }

    if (config.rp_model > 0) {
        a_rp_eci = forces[i]->get_eci_acceleration();
        a_rp_ecef = forces[i]->get_ecef_acceleration_in_eci();
        i++;

        // WARNING: Make sure TRR comes after radiation pressure class
        if (config.trr == 1 || config.trr == 2 || config.trr == 3) {
            a_trr = forces[i]->get_acceleration_in_eci();
            i++;
        }
    }

    if (config.third_body) {
        a_third_body = forces[i]->get_acceleration_in_eci();
        i++;
    }

    // Earth gravity is always on
    a_earth_gravity = forces[i]->get_acceleration_in_eci();

    // clang-format off
    output_buffer << std::scientific << std::setprecision(15)
                  << std::setw(22) << a_antenna_thrust.x << ", "
                  << std::setw(22) << a_antenna_thrust.y << ", "
                  << std::setw(22) << a_antenna_thrust.z << ", "
			      << std::setw(22) << a_antenna_thrust.length() << ", "
                  << std::setw(22) << a_drag.x << ", "
                  << std::setw(22) << a_drag.y << ", "
                  << std::setw(22) << a_drag.z << ", "
              	  << std::setw(22) << a_drag.length() << ", "
                  << std::setw(22) << a_gr_corrections.x << ", "
                  << std::setw(22) << a_gr_corrections.y << ", "
                  << std::setw(22) << a_gr_corrections.z << ", "
              	  << std::setw(22) << a_gr_corrections.length() << ", "
                  << std::setw(22) << a_lorentz.x << ", "
                  << std::setw(22) << a_lorentz.y << ", "
                  << std::setw(22) << a_lorentz.z << ", "
              	  << std::setw(22) << a_lorentz.length() << ", "
                  << std::setw(22) << a_rp_eci.x << ", "
                  << std::setw(22) << a_rp_eci.y << ", "
                  << std::setw(22) << a_rp_eci.z << ", "
              	  << std::setw(22) << a_rp_eci.length() << ", "
                  << std::setw(22) << a_rp_ecef.x << ", "
                  << std::setw(22) << a_rp_ecef.y << ", "
                  << std::setw(22) << a_rp_ecef.z << ", "
              	  << std::setw(22) << a_rp_ecef.length() << ", "
                  << std::setw(22) << a_trr.x << ", "
                  << std::setw(22) << a_trr.y << ", "
                  << std::setw(22) << a_trr.z << ", "
              	  << std::setw(22) << a_trr.length() << ", "
                  << std::setw(22) << a_third_body.x << ", "
                  << std::setw(22) << a_third_body.y << ", "
                  << std::setw(22) << a_third_body.z << ", "
              	  << std::setw(22) << a_third_body.length() << ", "
                  << std::setw(22) << a_earth_gravity.x << ", "
                  << std::setw(22) << a_earth_gravity.y << ", "
                  << std::setw(22) << a_earth_gravity.z << ", "
              	  << std::setw(22) << a_earth_gravity.length();
    // clang-format on
}

void Resident_space_object::store_max_individual_accelerations()
{
    size_t i = 0;
    size_t j = 0;

    if (config.antenna_thrust) {
        acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
        i++;
        j++;
    }
    if (config.gr_corrections) {
        acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
        i++;
        j++;
    }
    if (config.magnetic_model > 0) {
        acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
        i++;
        j++;
    }
    if (config.drag) {
        acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
        i++;
        j++;
    }

    if (config.rp_model > 0) {
        acc[j] = std::max(acc[j], forces[i]->get_eci_acceleration().length());
        j++;
        acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
        i++;
        j++;

        if (config.trr == 1 || config.trr == 2 || config.trr == 3) {
            acc[j] =
                std::max(acc[j], forces[i]->get_ecef_acceleration().length());
            i++;
            j++;
        }
    }

    if (config.third_body) {
        acc[j] = std::max(acc[j], forces[i]->get_eci_acceleration().length());
        i++;
        j++;
    }

    // Earth gravity is always on
    acc[j] = std::max(acc[j], forces[i]->get_ecef_acceleration().length());
}

void Resident_space_object::print_max_individual_accelerations()
{
    size_t j = 0;

    if (config.antenna_thrust) {
        std::cout << "Antenna Thrust: " << acc[j] * 1000.0 << std::endl;
        j++;
    }
    if (config.gr_corrections) {
        std::cout << "GR Correction: " << acc[j] * 1000.0 << std::endl;
        j++;
    }
    if (config.magnetic_model > 0) {
        std::cout << "Lorentz: " << acc[j] * 1000.0 << std::endl;
        j++;
    }
    if (config.drag) {
        std::cout << "Drag: " << acc[j] * 1000.0 << std::endl;
        j++;
    }

    if (config.rp_model > 0) {
        std::cout << "ECI Rad. Pressure: " << acc[j] * 1000.0 << std::endl;
        j++;
        std::cout << "ECEF Rad. Pressure: " << acc[j] * 1000.0 << std::endl;
        j++;

        if (config.trr == 1 || config.trr == 2 || config.trr == 3) {
            std::cout << "TRR: " << acc[j] * 1000.0 << std::endl;
            j++;
        }
    }

    if (config.third_body) {
        std::cout << "Third Body: " << acc[j] * 1000.0 << std::endl;
        j++;
    }

    std::cout << "Earth Gravity: " << acc[j] * 1000.0 << std::endl;
}

std::string Resident_space_object::solar_properties_header() const
{
    return " Beta Angle (rad) "
           ", "
           " EPS Angle (rad) "
           ", "
           "  Eclipse State  ";
}

void Resident_space_object::stream_solar_properties(
    std::stringstream &output_buffer) const
{
    output_buffer << std::fixed << std::setprecision(15) << std::setw(18)
                  << state->beta_angle << ", " << std::setw(17)
                  << state->eps_angle << ", " << std::setw(17)
                  << state->eclipse_state;
}

std::string Resident_space_object::atmos_properties_header() const
{
    return " Latitude (rad)   "
           ", "
           " Longitude (rad)  "
           ", "
           "  Altitude (km)    "
           ", "
           " Rho:10E-9 kg/m^3 ";
}

void Resident_space_object::stream_atmos_properties(
    std::stringstream &output_buffer) const
{
    output_buffer << ", " 
                  << std::fixed << std::setprecision(15) << std::setw(18)
                  << state->geodetic.lat << ", " << std::setw(18)
                  << state->geodetic.lon << ", " << std::setw(18)
                  << state->geodetic.alt << ", " << std::setw(18)
                  << state->atmos_density*10e9;
}

std::string Resident_space_object::why_bad_state() const
{
    if (is_state_bad()) {
        std::stringstream errors;
        errors << std::endl
               << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
               << std::endl
               << std::endl
               << "Warning: Simulation halted due to the following error"
               << (state->errors.size() > 1u ? "s" : "") << ":" << std::endl
               << std::endl;

        for (const auto &error : state->errors) {
            errors << error << std::endl << std::endl;
        }

        errors << std::endl;

        return errors.str();
    } else {
        return "";
    }
}

void Resident_space_object::print_properties() const
{
    Timetag start, end;
    start = config.initial_state.epoch;
    end = start;
    end.step(config.simulation_time);

    std::cout << std::endl;
    config.print();

    for (const auto &report : state->reports) {
        std::cout << report << std::endl;
    }

    std::cout << std::endl;

    std::cout << "Craft Properties:" << std::endl << std::endl;
    rso_const.print_properties();

    std::cout << "Starting at: (UTC) ";
    start.print_UTC_datestamp();

    std::cout << "  Ending at: (UTC) ";
    end.print_UTC_datestamp();
    std::cout << std::endl;

    std::cout << "The initial state vector is:" << std::endl;
    initial_state.print_state_vector();
    std::cout << std::endl;

    std::cout << "The initial Keplerian elements are:" << std::endl;
    initial_state.print_elements();
    std::cout << std::endl;

    std::cout << "The following force models are active:" << std::endl;
    config.print_switches(); // Shows which force models are on
    std::cout << std::endl;
}
