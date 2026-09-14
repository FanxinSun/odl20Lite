/*! @file main_fit_orbit_to_sp3_v3.cpp
 *  @author Santosh Bhattarai, reworked for command-line use
 *  @date 15 September 2026
 *  @brief Batch least-squares fit of an orbit to a precise ephemeris.
 *
 *  Usage:
 *      fit_orbit_to_sp3_v3 <config.txt> <reference.eci> [output-interval-s]
 *
 *  The reference file is the ECI text format SP3_to_eci writes. The initial
 *  state in the config is corrected until the propagated orbit best matches
 *  that reference, and the residuals are reported.
 *
 *  What the numbers mean. The residual RMS is how well this software's force
 *  models can reproduce an independently determined orbit over the arc, so it
 *  is the closest thing here to a validated accuracy figure. The formal
 *  uncertainty that follows is the covariance of the fitted initial state,
 *  sigma^2 * (Phi^T W Phi)^-1; it describes the fit, and is only as meaningful
 *  as the force models behind it - a poor model gives a confident wrong answer.
 */

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "../include/JPL_orbit_format.h"
#include "../include/Propagators.h"

namespace
{
//! Stop rather than iterate forever if the fit will not settle.
constexpr int max_iterations = 30;

//! Convergence thresholds on the state correction, in km and km/s.
constexpr double tol_position = 1.0E-7;
constexpr double tol_velocity = 1.0E-8;
} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <config.txt> <reference.eci> [output-interval-s]\n"
                  << "  e.g. " << argv[0]
                  << " ../res/configOPS_gps.txt ../output/g01.eci 900"
                  << std::endl;
        return 1;
    }

    const std::string config_path = argv[1];
    const std::string reference_path = argv[2];
    const double output_interval = (argc > 3) ? std::atof(argv[3]) : 900.0;

    Configuration config_ops(config_path);

    // The fit differentiates the trajectory with respect to the initial state
    // using the state transition matrix the propagator carries alongside it.
    // Only RKF7/8 and RK4 integrate it; under the others rso.phiM stays at the
    // identity it was initialised to, the normal equations are built from the
    // wrong partials, and the solution converges quietly to the wrong answer.
    // Refuse rather than produce that.
    if (config_ops.propagator != 3 && config_ops.propagator != 4) {
        std::cerr
            << "propagator = " << config_ops.propagator
            << " does not integrate the state transition matrix, which this\n"
               "fit needs. Use propagator = 3 (RKF7/8) or 4 (RK4).\n"
               "  0 Keplerian, 1 RK8/7 and 2 SGP4 leave it at the identity,\n"
               "  which would give a confident but meaningless solution."
            << std::endl;
        return 1;
    }

    Timetag start = config_ops.initial_state.epoch;

    config_ops.output_format = "store";
    std::string output_file = ""; // No output file, results are kept in memory

    std::vector<std::string> reference_files;
    reference_files.push_back(reference_path);

    JPL_orbit_format reference(reference_files);

    const size_t no_obs = reference.orbit_time_series.size();
    if (no_obs < 8) {
        std::cerr << "Only " << no_obs
                  << " reference states; too few to fit six parameters."
                  << std::endl;
        return 1;
    }

    config_ops.output_interval = output_interval;
    config_ops.simulation_time =
        (static_cast<double>(no_obs) - 1.0) * output_interval;

    Matrix6x1 Observation = Matrix6x1::Zero();
    Matrix6x1 Computed = Matrix6x1::Zero();
    Matrix6x1 OMC = Matrix6x1::Zero();
    Matrix6x6 Phi = Matrix6x6::Identity();

    Matrix6x6 Phi_t_Phi = Matrix6x6::Zero();
    Matrix6x1 Phi_t_L = Matrix6x1::Zero();

    Matrix6x1 x = Matrix6x1::Zero();
    Matrix6x1 x0 = Matrix6x1::Zero();
    Matrix6x1 dx0 = Matrix6x1::Zero();
    Matrix6x1 dx = Matrix6x1::Zero();

    // Velocities here are differentiated from SP3 positions rather than
    // measured, so they are downweighted by six orders of magnitude and the
    // solution is driven by the positions.
    Matrix6x6 W = Matrix6x6::Identity();
    W(3, 3) = 1.0 / (1000.0 * 1000.0);
    W(4, 4) = 1.0 / (1000.0 * 1000.0);
    W(5, 5) = 1.0 / (1000.0 * 1000.0);

    double L_t_L = 0.0;

    // Unweighted position residual statistics, which are what an accuracy
    // figure should be quoted from.
    double sum_sq_pos = 0.0;
    double sum_sq_axis[3] = {0.0, 0.0, 0.0};
    double max_pos = 0.0;
    size_t n_res = 0;

    std::unique_ptr<Propagators> propagator =
        Propagators::get_prop(config_ops, output_file);

    propagator->output.reserve_store(no_obs);

    State_vector my_state = propagator->rso.get_eci();
    x = convert_vector(my_state);
    x0 = x;

    std::cout << "Fitting " << no_obs << " reference states at "
              << output_interval << " s spacing (" << std::setprecision(4)
              << (static_cast<double>(no_obs - 1) * output_interval / 3600.0)
              << " h arc)" << std::endl;

    int iteration_num = 0;
    bool converged = false;

    while (iteration_num < max_iterations) {
        Phi_t_Phi = Matrix6x6::Zero();
        Phi_t_L = Matrix6x1::Zero();
        L_t_L = 0.0;
        sum_sq_pos = 0.0;
        sum_sq_axis[0] = sum_sq_axis[1] = sum_sq_axis[2] = 0.0;
        max_pos = 0.0;
        n_res = 0;

        x += dx0;
        State_vector new_state(x(0), x(1), x(2), x(3), x(4), x(5), start);

        // Recycle rso by resetting various properties
        propagator->rso.phiM = Matrix6x6::Identity();
        propagator->rso.initial_state = Keplerian_elements(
            new_state, static_cast<long double>(propagator->rso.get_GM()));
        propagator->rso.update_with_acc_and_deriv(new_state);
        propagator->output.clear_store();

        double sim_time = propagator->propagate();
        sgnlOPS::ignore(sim_time);

        auto sim_data = propagator->output.get_store();
        if (sim_data.size() < no_obs) {
            std::cerr << "Propagation produced " << sim_data.size()
                      << " states but " << no_obs
                      << " were expected; check step size and interval."
                      << std::endl;
            return 1;
        }

        State_vector sim_state;

        for (size_t i = 1; i < no_obs; ++i) {
            std::tie(sim_state, Phi) = sim_data[i];
            Computed = convert_vector(sim_state);
            Observation = convert_vector(reference.orbit_time_series[i]);
            OMC = Observation - Computed;

            Phi_t_Phi += Phi.transpose() * W * Phi;
            Phi_t_L += Phi.transpose() * W * OMC;
            L_t_L += OMC.transpose() * W * OMC;

            const double r2 =
                OMC(0) * OMC(0) + OMC(1) * OMC(1) + OMC(2) * OMC(2);
            sum_sq_pos += r2;
            for (int k = 0; k < 3; ++k) {
                sum_sq_axis[k] += OMC(k) * OMC(k);
            }
            if (std::sqrt(r2) > max_pos) {
                max_pos = std::sqrt(r2);
            }
            ++n_res;
        }

        dx = Phi_t_Phi.inverse() * Phi_t_L;

        std::cout << "  iteration " << std::setw(2) << iteration_num
                  << "   position RMS "
                  << std::fixed << std::setprecision(4)
                  << (1000.0 * std::sqrt(sum_sq_pos /
                                         static_cast<double>(n_res)))
                  << " m" << std::endl;

        if (std::abs(dx(0) - dx0(0)) < tol_position &&
            std::abs(dx(1) - dx0(1)) < tol_position &&
            std::abs(dx(2) - dx0(2)) < tol_position &&
            std::abs(dx(3) - dx0(3)) < tol_velocity &&
            std::abs(dx(4) - dx0(4)) < tol_velocity &&
            std::abs(dx(5) - dx0(5)) < tol_velocity) {
            converged = true;
            break;
        }

        dx0 = dx;
        iteration_num++;
    }

    if (!converged) {
        std::cerr << "\nWarning: the fit did not converge in " << max_iterations
                  << " iterations. Residuals below are from the last pass."
                  << std::endl;
    }

    // --- results ----------------------------------------------------------
    const double rms_pos =
        1000.0 * std::sqrt(sum_sq_pos / static_cast<double>(n_res));

    std::cout << "\n--------------- Orbit fit ---------------\n"
              << std::fixed << std::setprecision(4);
    std::cout << "  states fitted        : " << n_res << "\n"
              << "  converged            : " << (converged ? "yes" : "NO")
              << " (" << iteration_num << " iterations)\n"
              << "  position RMS         : " << rms_pos << " m\n"
              << "    x / y / z RMS      : "
              << 1000.0 * std::sqrt(sum_sq_axis[0] /
                                    static_cast<double>(n_res))
              << " / "
              << 1000.0 * std::sqrt(sum_sq_axis[1] /
                                    static_cast<double>(n_res))
              << " / "
              << 1000.0 * std::sqrt(sum_sq_axis[2] /
                                    static_cast<double>(n_res))
              << " m\n"
              << "  worst residual       : " << (1000.0 * max_pos) << " m\n";

    std::cout << "  total correction     : "
              << 1000.0 * (x - x0).head(3).norm() << " m position, "
              << 1000.0 * (x - x0).tail(3).norm() << " mm/s velocity\n";

    // Formal covariance of the estimated initial state. The normal matrix is
    // built from weighted residuals, so scale it by the variance of unit
    // weight to get something in km^2 and (km/s)^2.
    const double dof = static_cast<double>(6 * n_res) - 6.0;
    if (dof > 0.0) {
        const double variance_of_unit_weight = L_t_L / dof;
        Matrix6x6 covariance =
            variance_of_unit_weight * Phi_t_Phi.inverse();

        std::cout << "\n  formal 1-sigma on the fitted initial state\n"
                  << std::scientific << std::setprecision(4);
        const char *label[6] = {"x", "y", "z", "u", "v", "w"};
        for (int k = 0; k < 6; ++k) {
            const double sigma = std::sqrt(std::abs(covariance(k, k)));
            std::cout << "    " << label[k] << " : " << sigma
                      << ((k < 3) ? " km" : " km/s") << "\n";
        }
        std::cout << std::fixed << std::setprecision(4)
                  << "    position 1-sigma : "
                  << 1000.0 * std::sqrt(std::abs(covariance(0, 0)) +
                                        std::abs(covariance(1, 1)) +
                                        std::abs(covariance(2, 2)))
                  << " m\n";
        std::cout << "\n  Note: this is the precision of the fit, not its\n"
                     "  accuracy. It says how tightly the data pin the state\n"
                     "  down given these force models, not whether the force\n"
                     "  models are right. The residual RMS above is the honest\n"
                     "  measure of that.\n";
    }

    std::cout << "-----------------------------------------" << std::endl;

    return converged ? 0 : 2;
}
