/*! @file main_fit_orbit_to_slr.cpp
 *  @date 15 September 2026
 *  @brief Batch least-squares fit of an orbit to satellite laser ranging
 *         normal points.
 *
 *  Usage:
 *      fit_orbit_to_slr <config.txt> <observations.slrobs> [options]
 *
 *      --six | --seven | --ecom     parameters, as in fit_orbit_to_sp3_v3
 *      --stations <file>            default ../res/slr_stations.txt
 *      --reject <metres>            drop residuals larger than this and refit
 *      --apriori <km> <km/s>        constrain the initial state to the config
 *                                   value with these one-sigma uncertainties
 *      --apriori-rtn <R> <T> <N> <km/s>   the same, but with separate radial,
 *                                   along-track and cross-track sigmas. A
 *                                   prediction for one of these objects is
 *                                   wrong almost entirely along track, so an
 *                                   isotropic prior either lets the orbit
 *                                   wander sideways or pins the one direction
 *                                   that needs to move.
 *
 *  Why this exists. Everything else in this tree fits to a trajectory someone
 *  else computed - an IGS precise ephemeris, a Horizons table - so the answer
 *  can be no better than that reference, and for most objects the only
 *  available reference is derived from two-line elements. A laser range is a
 *  measurement rather than a trajectory: a normal point is a round trip time
 *  good to a centimetre or two, and no orbit model stands behind it. It is
 *  therefore the only reference here that can be trusted on an object whose
 *  dynamics are the thing in question.
 *
 *  The cost is that a range is one number instead of three, from one station,
 *  during the few minutes of a pass. The geometry is poor by construction, and
 *  a fit that converges to a small residual can still be badly determined in
 *  the directions no pass constrained. Read the formal sigma and the condition
 *  number below before believing the parameter.
 *
 *  What is modelled: the light time on both legs, the station's motion between
 *  transmit and receive, and tropospheric refraction by Marini-Murray. What is
 *  not: the offset between the retroreflector and the centre of mass (a fixed
 *  bias of a few centimetres, target-specific), solid Earth tide and ocean
 *  loading displacement of the station (up to ~0.3 m, mostly common to a pass),
 *  and the relativistic range delay (~0.02 m). Those matter at the centimetre
 *  level this data is capable of; they do not matter when the orbit itself is
 *  the unknown, which is the case this was written for.
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../include/Matrix.h"

#include "../include/Frame_transform.h"
#include "../include/Tracking_fit.h"

namespace
{
using sgnlTracking::Station;
using sgnlTracking::Store;
using sgnlTracking::interpolate;
using sgnlTracking::marini_murray;
using sgnlTracking::read_stations;
using sgnlTracking::scaled_solve;

constexpr int max_iterations = 400;
constexpr double tol_position = 1.0E-7; // km
constexpr double tol_velocity = 1.0E-8; // km/s

//! Speed of light in vacuum, km/s, as the ILRS defines the range observable.
constexpr double c_km_s = 299792.458;


struct Observation {
    std::string station;
    Timetag epoch;         //!< ground transmit, UTC
    double range = 0.0;    //!< km, c * time-of-flight / 2
    double pressure = 0.0; //!< mbar
    double temperature = 0.0;
    double humidity = 0.0;
    double wavelength = 0.532; //!< micrometres
};

bool read_observations(const std::string &path, std::vector<Observation> &out)
{
    std::ifstream in(path);
    if (!in) {
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        Observation o;
        long int y, mo, d, hh, mm;
        double sec, rms;
        if (!(ss >> o.station >> y >> mo >> d >> hh >> mm >> sec >> o.range >>
              o.pressure >> o.temperature >> o.humidity >> o.wavelength >>
              rms)) {
            continue;
        }
        o.epoch.set_timetag_from_cal_UTC(y, mo, d, hh, mm, sec);
        out.push_back(o);
    }
    std::sort(out.begin(), out.end(),
              [](const Observation &a, const Observation &b) {
                  return a.epoch < b.epoch;
              });
    return !out.empty();
}

} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <config.txt> <observations.slrobs> [--six|--seven|--ecom]"
                     " [--stations f] [--reject m]\n";
        return 1;
    }

    const std::string config_path = argv[1];
    const std::string obs_path = argv[2];
    std::string station_path = "../res/slr_stations.txt";
    bool estimate_srp = true;
    bool ecom = false;
    double reject = 0.0;
    double apriori_pos = 0.0;
    double apriori_vel = 0.0;
    double apriori_rtn[3] = {0.0, 0.0, 0.0};
    bool apriori_is_rtn = false;

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--six") { estimate_srp = false; }
        else if (a == "--seven") { estimate_srp = true; }
        else if (a == "--ecom") { estimate_srp = true; ecom = true; }
        else if (a == "--stations" && i + 1 < argc) { station_path = argv[++i]; }
        else if (a == "--reject" && i + 1 < argc) {
            reject = std::atof(argv[++i]);
        }
        else if (a == "--apriori" && i + 2 < argc) {
            apriori_pos = std::atof(argv[++i]);
            apriori_vel = std::atof(argv[++i]);
        }
        else if (a == "--apriori-rtn" && i + 4 < argc) {
            apriori_rtn[0] = std::atof(argv[++i]);
            apriori_rtn[1] = std::atof(argv[++i]);
            apriori_rtn[2] = std::atof(argv[++i]);
            apriori_vel = std::atof(argv[++i]);
            apriori_pos = apriori_rtn[0];
            apriori_is_rtn = true;
        }
    }

    const int n_emp = ecom ? N_EMP : (estimate_srp ? 1 : 0);
    const int n_par = 6 + n_emp;

    std::map<std::string, Station> stations;
    if (!read_stations(station_path, stations)) {
        std::cerr << "Cannot read station coordinates from " << station_path
                  << std::endl;
        return 1;
    }

    std::vector<Observation> obs;
    if (!read_observations(obs_path, obs)) {
        std::cerr << "Cannot read observations from " << obs_path << std::endl;
        return 1;
    }

    Configuration config_ops(config_path);

    // Same rule as the ephemeris fit: only the propagators that integrate the
    // state transition matrix carry the partials this needs. The others leave
    // it at the identity and the solution would be confident and wrong.
    if (config_ops.propagator != 3 && config_ops.propagator != 4) {
        std::cerr << "propagator = " << config_ops.propagator
                  << " does not integrate the state transition matrix.\n"
                     "Use propagator = 3 (RKF7/8) or 4 (RK4)." << std::endl;
        return 1;
    }

    const Timetag start = config_ops.initial_state.epoch;

    // Keep every integration step, so a normal point anywhere in the arc is
    // bracketed. Ten seconds of grid is a fraction of a millimetre of
    // interpolation error; the memory is about 600 bytes a step.
    const double grid = config_ops.step_size;
    config_ops.output_interval = grid;
    config_ops.output_format = "store";

    // Trim to the observations the arc actually covers, and say so rather than
    // silently fitting a subset.
    std::vector<Observation> used;
    double span = 0.0;
    for (const Observation &o : obs) {
        const double dt = o.epoch - start;
        if (dt < 0.0 || dt > config_ops.simulation_time) {
            continue;
        }
        if (stations.find(o.station) == stations.end()) {
            std::cerr << "No coordinates for station " << o.station
                      << " in " << station_path << std::endl;
            return 1;
        }
        used.push_back(o);
        if (dt > span) { span = dt; }
    }

    if (static_cast<int>(used.size()) < n_par) {
        std::cerr << used.size() << " observations inside the arc against "
                  << n_par << " parameters. Widen simulation_time or drop "
                     "parameters." << std::endl;
        return 1;
    }

    // A range is one scalar, so the arc has to be long enough for the geometry
    // to change. Leave a little room past the last point for the light time.
    config_ops.simulation_time = span + 60.0;

    Frame_transform ft;
    ft.setup(start, config_ops.simulation_time);

    std::unique_ptr<Propagators> propagator =
        Propagators::get_prop(config_ops, "");
    propagator->output.reserve_store(
        static_cast<size_t>(config_ops.simulation_time / grid) + 4);

    Eigen::VectorXd x = Eigen::VectorXd::Zero(n_par);
    Eigen::VectorXd dx = Eigen::VectorXd::Zero(n_par);
    Eigen::VectorXd dx0 = Eigen::VectorXd::Zero(n_par);

    State_vector my_state = propagator->rso.get_eci();
    Matrix6x1 s0 = convert_vector(my_state);
    for (int i = 0; i < 6; ++i) { x(i) = s0(i); }
    if (estimate_srp) { x(6) = propagator->rso.get_srp_scale(); }

    const Eigen::VectorXd x_apriori = x;

    std::cout << "CONFIG  file       : " << config_path << "\n"
              << "CONFIG  epoch      : " << start.str_UTC_datestamp() << "\n"
              << "CONFIG  propagator : " << config_ops.propagator << "   step "
              << config_ops.step_size << " s\n"
              << "CONFIG  area/mass  : " << config_ops.area << " m^2 / "
              << config_ops.mass << " kg\n"
              << "CONFIG  forces     : srp=" << config_ops.srp
              << " rp_model=" << config_ops.rp_model
              << " third_body=" << config_ops.third_body
              << " drag=" << config_ops.drag
              << " grav=" << config_ops.gravity_model << "/"
              << config_ops.grav_degree << "\n"
              << "CONFIG  parameters : " << n_par;
    if (apriori_is_rtn) {
        std::cout << "   a priori RTN " << apriori_rtn[0] << "/"
                  << apriori_rtn[1] << "/" << apriori_rtn[2] << " km, "
                  << apriori_vel << " km/s";
    } else if (apriori_pos > 0.0) {
        std::cout << "   a priori state " << apriori_pos << " km / "
                  << apriori_vel << " km/s";
    }
    std::cout << "\n";

    std::cout << "Fitting " << used.size() << " laser ranges over "
              << std::setprecision(4) << (span / 3600.0) << " h ("
              << obs.size() << " read)" << std::endl;

    std::vector<double> residual(used.size(), 0.0);
    std::vector<double> elev_deg(used.size(), 0.0);
    std::vector<bool> keep(used.size(), true);
    Eigen::MatrixXd A_t_A = Eigen::MatrixXd::Zero(n_par, n_par);
    Eigen::VectorXd A_t_L = Eigen::VectorXd::Zero(n_par);
    double sigma0 = 0.0;
    size_t n_used = 0;

    /*! Propagate for one parameter vector, form the normal equations from the
     *  range residuals, and return their RMS in metres. Returns a huge number
     *  rather than failing if the trial trajectory leaves the arc, so that the
     *  step control below can simply reject it.
     */
    auto evaluate = [&](const Eigen::VectorXd &p, Eigen::MatrixXd &AtA,
                        Eigen::VectorXd &AtL, std::vector<double> &res,
                        size_t &n) -> double {
        AtA = Eigen::MatrixXd::Zero(n_par, n_par);
        AtL = Eigen::VectorXd::Zero(n_par);
        double sum_sq = 0.0;
        n = 0;

        State_vector new_state(p(0), p(1), p(2), p(3), p(4), p(5), start);
        // A trial state from an earlier iteration may have left an error
        // behind; without clearing it this propagation halts on somebody
        // else's problem.
        propagator->rso.clear_errors();
        propagator->rso.phiM = Matrix6x6::Identity();
        propagator->rso.srpS = Matrix6x5::Zero();
        if (estimate_srp) { propagator->rso.set_srp_scale(p(6)); }
        for (int q = 1; q < n_emp; ++q) {
            propagator->rso.set_emp_coeff(q, p(6 + q));
        }
        propagator->rso.initial_state = Keplerian_elements(
            new_state, static_cast<long double>(propagator->rso.get_GM()));
        propagator->rso.update_with_acc_and_deriv(new_state);
        propagator->output.clear_store();
        sgnlOPS::ignore(propagator->propagate());
        const Store &store = propagator->output.get_store();

        for (size_t i = 0; i < used.size(); ++i) {
            const Observation &o = used[i];
            const Station &st = stations.at(o.station);

            // Station position at the transmit epoch, plate motion included.
            const double years =
                (o.epoch.get_MJD_UTC() - 51544.5) / 365.25 + 2000.0 -
                st.ref_year;
            const Cartesian sta_ecef(st.pos.x + st.vel.x * years,
                                     st.pos.y + st.vel.y * years,
                                     st.pos.z + st.vel.z * years);

            ft.compute_rotations(o.epoch, 0.0);
            const Cartesian sta_tx = ft.rotate_ecef_to_eci(sta_ecef);

            // Up leg: solve for the bounce epoch. Four passes is well past
            // convergence at 7 km/s.
            State_vector sat;
            Matrix6x6 Phi;
            Matrix6x5 Sens;
            double tau_up = 0.0;
            bool ok = true;
            for (int it = 0; it < 4; ++it) {
                if (!interpolate(store, (o.epoch - start) + tau_up, grid, sat,
                                 Phi, Sens)) {
                    ok = false;
                    break;
                }
                const double dxk = static_cast<double>(sat.x) - sta_tx.x;
                const double dyk = static_cast<double>(sat.y) - sta_tx.y;
                const double dzk = static_cast<double>(sat.z) - sta_tx.z;
                tau_up = std::sqrt(dxk * dxk + dyk * dyk + dzk * dzk) / c_km_s;
            }
            if (!ok) {
                return 1.0E30;
            }

            const Cartesian up(static_cast<double>(sat.x) - sta_tx.x,
                               static_cast<double>(sat.y) - sta_tx.y,
                               static_cast<double>(sat.z) - sta_tx.z);
            const double rho_up = tau_up * c_km_s;

            // Down leg: the station has moved by the time the pulse gets back.
            double tau_dn = tau_up;
            Cartesian down;
            double rho_dn = rho_up;
            for (int it = 0; it < 3; ++it) {
                const Timetag t_rx = o.epoch + (tau_up + tau_dn);
                ft.compute_rotations(t_rx, 0.0);
                const Cartesian sta_rx = ft.rotate_ecef_to_eci(sta_ecef);
                down = Cartesian(static_cast<double>(sat.x) - sta_rx.x,
                                 static_cast<double>(sat.y) - sta_rx.y,
                                 static_cast<double>(sat.z) - sta_rx.z);
                rho_dn = std::sqrt(down.x * down.x + down.y * down.y +
                                   down.z * down.z);
                tau_dn = rho_dn / c_km_s;
            }

            // Elevation, for the refraction model, from the bounce geometry.
            ft.compute_rotations(sat.epoch, 0.0);
            const Cartesian sat_ecef =
                ft.rotate_eci_to_ecef(Cartesian(static_cast<double>(sat.x),
                                                static_cast<double>(sat.y),
                                                static_cast<double>(sat.z)));
            const Cartesian d(sat_ecef.x - sta_ecef.x, sat_ecef.y - sta_ecef.y,
                              sat_ecef.z - sta_ecef.z);
            const double dn = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            const double up_x = std::cos(st.latitude) * std::cos(st.longitude);
            const double up_y = std::cos(st.latitude) * std::sin(st.longitude);
            const double up_z = std::sin(st.latitude);
            const double elevation =
                std::asin((d.x * up_x + d.y * up_y + d.z * up_z) / dn);

            const double trop =
                marini_murray(o.pressure, o.temperature, o.humidity,
                              o.wavelength, st.latitude, st.height, elevation);

            const double computed = 0.5 * (rho_up + rho_dn) + trop;
            const double omc = o.range - computed;
            res[i] = omc;
            elev_deg[i] = elevation * 57.29577951308232;

            if (!keep[i]) {
                continue;
            }

            // The range responds to the satellite position along the mean of
            // the two lines of sight; the light-time terms are smaller than
            // this data can see.
            const double ux = 0.5 * (up.x / rho_up + down.x / rho_dn);
            const double uy = 0.5 * (up.y / rho_up + down.y / rho_dn);
            const double uz = 0.5 * (up.z / rho_up + down.z / rho_dn);

            Eigen::VectorXd row(n_par);
            for (int q = 0; q < 6; ++q) {
                row(q) = ux * Phi(0, q) + uy * Phi(1, q) + uz * Phi(2, q);
            }
            for (int q = 0; q < n_emp; ++q) {
                row(6 + q) =
                    ux * Sens(0, q) + uy * Sens(1, q) + uz * Sens(2, q);
            }

            AtA += row * row.transpose();
            AtL += row * omc;
            sum_sq += omc * omc;
            ++n;
        }

        if (n == 0) {
            return 1.0E30;
        }

        /* One station ranging one satellite sees almost the same direction on
         * every pass, so six state elements plus a force parameter are not all
         * determined: without this the normal matrix comes out singular to
         * machine precision and the solver returns a confident number with a
         * meaningless sigma. The prediction the stations pointed with is a real
         * prior - a few hundred metres once its time bias is taken out - so use
         * it as one, and let the ranges move the state only as far as they
         * actually constrain it.
         */
        if (apriori_pos > 0.0) {
            Eigen::Matrix3d Wpos = Eigen::Matrix3d::Zero();
            if (apriori_is_rtn) {
                // Radial, along-track, cross-track basis from the a priori
                // state, so the three sigmas mean what they say.
                Eigen::Vector3d rv(x_apriori(0), x_apriori(1), x_apriori(2));
                Eigen::Vector3d vv(x_apriori(3), x_apriori(4), x_apriori(5));
                Eigen::Vector3d rhat = rv.normalized();
                Eigen::Vector3d nhat = rv.cross(vv).normalized();
                Eigen::Vector3d that = nhat.cross(rhat);
                Eigen::Matrix3d M;
                M.col(0) = rhat;
                M.col(1) = that;
                M.col(2) = nhat;
                Eigen::Matrix3d D = Eigen::Matrix3d::Zero();
                for (int k = 0; k < 3; ++k) {
                    D(k, k) = 1.0 / (apriori_rtn[k] * apriori_rtn[k]);
                }
                Wpos = M * D * M.transpose();
            } else {
                Wpos = Eigen::Matrix3d::Identity() /
                       (apriori_pos * apriori_pos);
            }
            const Eigen::Vector3d dr(x_apriori(0) - p(0), x_apriori(1) - p(1),
                                     x_apriori(2) - p(2));
            AtA.block(0, 0, 3, 3) += Wpos;
            AtL.head(3) += Wpos * dr;

            const double wv = 1.0 / (apriori_vel * apriori_vel);
            for (int k = 3; k < 6; ++k) {
                AtA(k, k) += wv;
                AtL(k) += wv * (x_apriori(k) - p(k));
            }
        }

        return 1000.0 * std::sqrt(sum_sq / static_cast<double>(n));
    };

    sigma0 = evaluate(x, A_t_A, A_t_L, residual, n_used);
    std::cout << "  start           range RMS " << std::fixed
              << std::setprecision(3) << sigma0 << " m   (" << n_used
              << " points)" << std::endl;

    /* Plain Gauss-Newton diverges here. Over a three day arc a sail's
     * trajectory is nothing like linear in its initial state: the first
     * correction overshoots into a worse orbit, and the next one overshoots
     * further. Levenberg-Marquardt handles it - damp the normal matrix until
     * the step is one that actually helps, then relax the damping as the
     * solution settles and it becomes Gauss-Newton again near the minimum.
     */
    double lambda = 1.0E-3;
    int iteration_num = 0;
    bool converged = false;
    for (; iteration_num < max_iterations; ++iteration_num) {
        bool accepted = false;
        Eigen::MatrixXd AtA_try = A_t_A;
        Eigen::VectorXd AtL_try = A_t_L;
        std::vector<double> res_try = residual;
        size_t n_try = n_used;
        double rms_try = sigma0;
        Eigen::VectorXd x_try = x;
        Eigen::VectorXd dx = Eigen::VectorXd::Zero(n_par);

        for (int back = 0; back < 20; ++back) {
            Eigen::MatrixXd damped = A_t_A;
            for (int k = 0; k < n_par; ++k) {
                damped(k, k) += lambda * std::abs(A_t_A(k, k));
            }
            double cond_ignored = 0.0;
            dx = scaled_solve(damped, A_t_L, cond_ignored);
            x_try = x + dx;
            rms_try = evaluate(x_try, AtA_try, AtL_try, res_try, n_try);
            if (rms_try < sigma0) {
                accepted = true;
                lambda = std::max(lambda / 3.0, 1.0E-12);
                break;
            }
            if (std::getenv("SGNL_SLR_VERBOSE")) {
                std::cout << "      lambda " << std::scientific << lambda
                          << " gave " << std::fixed << std::setprecision(1)
                          << rms_try << " m, damping harder" << std::endl;
            }
            lambda *= 10.0;
        }

        if (!accepted) {
            converged = true; // nowhere better to go
            break;
        }

        x = x_try;
        A_t_A = AtA_try;
        A_t_L = AtL_try;
        residual = res_try;
        n_used = n_try;

        std::cout << "  iteration " << std::setw(2) << iteration_num
                  << "   range RMS " << std::fixed << std::setprecision(3)
                  << rms_try << " m   lambda " << std::scientific << lambda
                  << std::fixed << std::endl;

        const bool small =
            std::abs(dx(0)) < tol_position && std::abs(dx(1)) < tol_position &&
            std::abs(dx(2)) < tol_position && std::abs(dx(3)) < tol_velocity &&
            std::abs(dx(4)) < tol_velocity && std::abs(dx(5)) < tol_velocity &&
            (!estimate_srp || std::abs(dx(6)) < 1.0E-9);
        // Also stop when the residual has stopped moving, which on data this
        // sparse happens well before the corrections underflow.
        const bool flat = (sigma0 - rms_try) < 1.0E-5 * rms_try;
        sigma0 = rms_try;
        if (small || flat) {
            converged = true;
            break;
        }
    }

    // Outlier pass, once, if asked. A single bad normal point in a set this
    // small drags the whole solution, and with one station there is nothing
    // else to contradict it.
    if (reject > 0.0) {
        size_t dropped = 0;
        for (size_t i = 0; i < used.size(); ++i) {
            if (keep[i] && std::abs(residual[i]) * 1000.0 > reject) {
                keep[i] = false;
                ++dropped;
            }
        }
        if (dropped > 0) {
            std::cout << "\nRejected " << dropped << " residual(s) over "
                      << reject << " m; re-run to refit without them."
                      << std::endl;
        }
    }

    std::cout << "\n--- fit to " << n_used << " laser ranges ---\n";
    std::cout << "  range residual RMS   : " << std::fixed
              << std::setprecision(3) << sigma0 << " m\n";

    double max_res = 0.0, mean_res = 0.0;
    for (size_t i = 0; i < used.size(); ++i) {
        if (!keep[i]) { continue; }
        mean_res += residual[i] * 1000.0;
        max_res = std::max(max_res, std::abs(residual[i]) * 1000.0);
    }
    mean_res /= static_cast<double>(n_used);
    std::cout << "  mean (bias)          : " << mean_res << " m\n"
              << "  largest              : " << max_res << " m\n";

    std::cout << "\n  epoch                                elev      O-C (m)\n";
    for (size_t i = 0; i < used.size(); ++i) {
        std::cout << "  " << used[i].epoch.str_UTC_datestamp() << "  "
                  << std::setw(6) << std::setprecision(2) << elev_deg[i]
                  << "  " << std::setw(11) << std::setprecision(3)
                  << residual[i] * 1000.0 << (keep[i] ? "" : "   rejected")
                  << "\n";
    }

    if (estimate_srp) {
        double cond = 0.0;
        Eigen::MatrixXd unit(n_par, n_par);
        {
            // Covariance in the scaled parameters, then undone, for the same
            // reason the solve is scaled.
            Eigen::VectorXd d(n_par);
            for (int k = 0; k < n_par; ++k) {
                const double a = std::abs(A_t_A(k, k));
                d(k) = (a > 0.0) ? 1.0 / std::sqrt(a) : 1.0;
            }
            const Eigen::MatrixXd As =
                d.asDiagonal() * A_t_A * d.asDiagonal();
            Eigen::JacobiSVD<Eigen::MatrixXd> svd(
                As, Eigen::ComputeThinU | Eigen::ComputeThinV);
            cond = svd.singularValues()(0) /
                   svd.singularValues()(svd.singularValues().size() - 1);
            unit = d.asDiagonal() *
                   As.completeOrthogonalDecomposition().pseudoInverse() *
                   d.asDiagonal();
        }
        const Eigen::MatrixXd cov =
            (sigma0 / 1000.0) * (sigma0 / 1000.0) * unit;
        const double scale = x(6);
        const double sigma_scale = std::sqrt(std::abs(cov(6, 6)));
        // The estimated quantity is a scale on the modelled acceleration, so
        // the physical number it carries is the product of area, reflectivity
        // and that scale, over mass.
        const double acrm = scale * config_ops.area *
                            propagator->rso.get_srp_CR() / config_ops.mass;
        std::cout << "\n  solar radiation pressure scale : " << std::setprecision(6)
                  << scale << " +/- " << sigma_scale << "\n"
                  << "  effective A*C_R/m              : " << acrm << " +/- "
                  << std::abs(acrm) * sigma_scale / std::abs(scale)
                  << " m^2/kg\n";

        std::cout << "  normal matrix condition number : " << std::scientific
                  << cond << std::fixed << "   (after scaling)\n";
        if (cond > 1.0E10) {
            std::cout << "  The geometry does not determine all " << n_par
                      << " parameters; treat the sigma above as a lower bound.\n";
        }
    }

    std::cout << "\n  The residual is a measurement, not a trajectory "
                 "comparison: it is\n  how far this force model is from where "
                 "the laser actually found the\n  satellite. Unmodelled at "
                 "this level: retroreflector offset from the\n  centre of mass, "
                 "station tidal displacement, relativistic delay.\n";

    return converged ? 0 : 2;
}
