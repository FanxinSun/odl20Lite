/*! @file main_fit_orbit_to_angles.cpp
 *  @date 16 September 2026
 *  @brief Batch least-squares fit of an orbit to optical angles - right
 *         ascension and declination measured against the star background.
 *
 *  Usage:
 *      fit_orbit_to_angles <config.txt> <observations.angles> [options]
 *
 *      --six | --seven | --ecom     parameters, as in the other fits
 *      --sites <file>               default ../res/obs_sites.txt
 *      --apriori <km> <km/s>        constrain the initial state
 *      --apriori-rtn <R> <T> <N> <km/s>   the same, per direction
 *      --sigma <arcsec>             observation weight, default 60
 *
 *  Why this and not the laser fit. Satellite laser ranging measures a range to
 *  a centimetre, and it is the best data anyone has - but it only works on an
 *  object that was built to be ranged, with a retroreflector on it and its
 *  orbit known well enough to point at. The objects this software is aimed at
 *  are none of those things: no transponder, no reflector, no cooperation, and
 *  frequently no reliable orbit to start from. What exists for them is angles -
 *  someone photographed a moving dot against stars and measured where it was.
 *
 *  An angle is a much weaker measurement than a range. It says nothing at all
 *  about distance, so range and along-track position are not observed directly
 *  and have to come out of how the direction changes over an arc. Two numbers
 *  per observation instead of one helps; sparse coverage from one site hurts.
 *  Expect to need a prior, and read the condition number.
 *
 *  What is modelled: light time, and the observer's position in the inertial
 *  frame at the instant of observation. What is not, because the measurement is
 *  differential against stars in the same field and the same displacement
 *  applies to both: annual and diurnal aberration, and refraction. That
 *  argument holds to well under an arcsecond, which is far below what visual
 *  astrometry delivers; it would NOT hold for absolute pointing.
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
using sgnlTracking::read_sites;
using sgnlTracking::scaled_solve;

constexpr int max_iterations = 400;
constexpr double tol_position = 1.0E-7; // km
constexpr double tol_velocity = 1.0E-8; // km/s

constexpr double c_km_s = 299792.458;
constexpr double pi = 3.14159265358979323846;
constexpr double rad_per_arcsec = pi / (180.0 * 3600.0);

struct Observation {
    std::string site;
    Timetag epoch;      //!< when the light arrived, UTC
    double ra = 0.0;    //!< radians, J2000
    double dec = 0.0;   //!< radians, J2000
    double sigma = 0.0; //!< radians, as reported by the observer
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
        double sec, ra_deg, dec_deg, sigma_arcsec;
        if (!(ss >> o.site >> y >> mo >> d >> hh >> mm >> sec >> ra_deg >>
              dec_deg >> sigma_arcsec)) {
            continue;
        }
        o.epoch.set_timetag_from_cal_UTC(y, mo, d, hh, mm, sec);
        o.ra = ra_deg * pi / 180.0;
        o.dec = dec_deg * pi / 180.0;
        o.sigma = sigma_arcsec * rad_per_arcsec;
        out.push_back(o);
    }
    std::sort(out.begin(), out.end(),
              [](const Observation &a, const Observation &b) {
                  return a.epoch < b.epoch;
              });
    return !out.empty();
}

//! Wrap a right ascension difference into (-pi, pi].
double wrap(double d)
{
    while (d > pi) { d -= 2.0 * pi; }
    while (d <= -pi) { d += 2.0 * pi; }
    return d;
}
} // namespace

int main(int argc, char *argv[])
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <config.txt> <observations.angles>"
                     " [--six|--seven|--ecom] [--sites f]\n"
                     "        [--apriori km km/s | --apriori-rtn R T N km/s]"
                     " [--sigma arcsec]\n";
        return 1;
    }

    const std::string config_path = argv[1];
    const std::string obs_path = argv[2];
    std::string site_path = "../res/obs_sites.txt";
    bool estimate_srp = true;
    bool ecom = false;
    double apriori_pos = 0.0;
    double apriori_vel = 0.0;
    double apriori_rtn[3] = {0.0, 0.0, 0.0};
    bool apriori_is_rtn = false;
    double sigma_default = 60.0; // arcsec

    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--six") { estimate_srp = false; }
        else if (a == "--seven") { estimate_srp = true; }
        else if (a == "--ecom") { estimate_srp = true; ecom = true; }
        else if (a == "--sites" && i + 1 < argc) { site_path = argv[++i]; }
        else if (a == "--sigma" && i + 1 < argc) {
            sigma_default = std::atof(argv[++i]);
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

    std::map<std::string, Station> sites;
    if (!read_sites(site_path, sites)) {
        std::cerr << "Cannot read observing sites from " << site_path
                  << std::endl;
        return 1;
    }

    std::vector<Observation> obs;
    if (!read_observations(obs_path, obs)) {
        std::cerr << "Cannot read observations from " << obs_path << std::endl;
        return 1;
    }

    Configuration config_ops(config_path);

    if (config_ops.propagator != 3 && config_ops.propagator != 4) {
        std::cerr << "propagator = " << config_ops.propagator
                  << " does not integrate the state transition matrix.\n"
                     "Use propagator = 3 (RKF7/8) or 4 (RK4)." << std::endl;
        return 1;
    }

    const Timetag start = config_ops.initial_state.epoch;
    const double grid = config_ops.step_size;
    config_ops.output_interval = grid;
    config_ops.output_format = "store";

    std::vector<Observation> used;
    double span = 0.0;
    for (const Observation &o : obs) {
        const double dt = o.epoch - start;
        if (dt < 0.0 || dt > config_ops.simulation_time) {
            continue;
        }
        // The model steps back from each observation by the light time, so an
        // observation sitting on the first integration step has nothing behind
        // it to interpolate from. Refuse rather than return a huge residual
        // that looks like a bad orbit.
        if (dt < 1.0) {
            std::cerr << "The first observation is " << dt
                      << " s after the config epoch. Start the arc at least a "
                         "second earlier; the light time is stepped backwards."
                      << std::endl;
            return 1;
        }
        if (sites.find(o.site) == sites.end()) {
            std::cerr << "No coordinates for site " << o.site << " in "
                      << site_path << std::endl;
            return 1;
        }
        used.push_back(o);
        if (dt > span) { span = dt; }
    }

    // Two rows per observation, so the arithmetic floor is half what a range
    // fit needs. Conditioning, not counting, decides whether it is usable.
    if (2 * static_cast<int>(used.size()) < n_par) {
        std::cerr << used.size() << " observations (" << 2 * used.size()
                  << " rows) inside the arc against " << n_par
                  << " parameters." << std::endl;
        return 1;
    }

    config_ops.simulation_time = span + 60.0;

    Frame_transform ft;
    ft.setup(start, config_ops.simulation_time);

    std::unique_ptr<Propagators> propagator =
        Propagators::get_prop(config_ops, "");
    propagator->output.reserve_store(
        static_cast<size_t>(config_ops.simulation_time / grid) + 4);

    Eigen::VectorXd x = Eigen::VectorXd::Zero(n_par);
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
              << " drag_coeff=" << config_ops.drag_coeff
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

    std::cout << "Fitting " << used.size() << " optical angles over "
              << std::setprecision(4) << (span / 86400.0) << " d ("
              << obs.size() << " read)" << std::endl;

    std::vector<double> res_ra(used.size(), 0.0);
    std::vector<double> res_dec(used.size(), 0.0);
    std::vector<double> res_range(used.size(), 0.0);
    Eigen::MatrixXd A_t_A = Eigen::MatrixXd::Zero(n_par, n_par);
    Eigen::VectorXd A_t_L = Eigen::VectorXd::Zero(n_par);
    double sigma0 = 0.0;
    size_t n_used = 0;

    /*! Propagate for one parameter vector and form the normal equations from
     *  the angular residuals. Returns their RMS in arcseconds.
     */
    auto evaluate = [&](const Eigen::VectorXd &p, Eigen::MatrixXd &AtA,
                        Eigen::VectorXd &AtL, std::vector<double> &rra,
                        std::vector<double> &rdec, std::vector<double> &rrng,
                        size_t &n) -> double {
        AtA = Eigen::MatrixXd::Zero(n_par, n_par);
        AtL = Eigen::VectorXd::Zero(n_par);
        double sum_sq = 0.0;
        n = 0;

        State_vector new_state(p(0), p(1), p(2), p(3), p(4), p(5), start);
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
            const Station &st = sites.at(o.site);

            ft.compute_rotations(o.epoch, 0.0);
            const Cartesian obs_eci = ft.rotate_ecef_to_eci(st.pos);

            // The light left the satellite before it arrived, so step BACK from
            // the observation epoch. A range fit steps forward from a transmit
            // time; getting the sign wrong here is 22 m of along track, which
            // at a thousand kilometres is five arcseconds and would pass
            // unnoticed in visual data.
            State_vector sat;
            Matrix6x6 Phi;
            Matrix6x5 Sens;
            double tau = 0.0;
            bool ok = true;
            for (int it = 0; it < 4; ++it) {
                if (!interpolate(store, (o.epoch - start) - tau, grid, sat, Phi,
                                 Sens)) {
                    ok = false;
                    break;
                }
                const double dx = static_cast<double>(sat.x) - obs_eci.x;
                const double dy = static_cast<double>(sat.y) - obs_eci.y;
                const double dz = static_cast<double>(sat.z) - obs_eci.z;
                tau = std::sqrt(dx * dx + dy * dy + dz * dz) / c_km_s;
            }
            if (!ok) {
                return 1.0E30;
            }

            const double dx = static_cast<double>(sat.x) - obs_eci.x;
            const double dy = static_cast<double>(sat.y) - obs_eci.y;
            const double dz = static_cast<double>(sat.z) - obs_eci.z;
            const double rho = std::sqrt(dx * dx + dy * dy + dz * dz);

            const double ra_c = std::atan2(dy, dx);
            const double dec_c = std::asin(dz / rho);

            // Residuals on the sky: the right ascension one carries cos(dec) so
            // that both are angles on the sphere and can share a weight.
            const double d_ra = wrap(o.ra - ra_c) * std::cos(dec_c);
            const double d_dec = o.dec - dec_c;
            rra[i] = d_ra;
            rdec[i] = d_dec;
            rrng[i] = rho;

            // Unit vectors along increasing right ascension and declination.
            const double sa = std::sin(ra_c), ca = std::cos(ra_c);
            const double sd = std::sin(dec_c), cd = std::cos(dec_c);
            const double e_a[3] = {-sa, ca, 0.0};
            const double e_d[3] = {-sd * ca, -sd * sa, cd};

            // d(angle)/d(position) is the component across the line of sight
            // divided by the range - which is where the weakness of an angle
            // comes from: nothing in it responds to moving along the sight line.
            const double w = 1.0 / (o.sigma > 0.0 ? o.sigma
                                                  : sigma_default *
                                                        rad_per_arcsec);

            Eigen::VectorXd row_a(n_par);
            Eigen::VectorXd row_d(n_par);
            for (int q = 0; q < 6; ++q) {
                double pa = 0.0, pd = 0.0;
                for (int k = 0; k < 3; ++k) {
                    pa += e_a[k] * Phi(k, q);
                    pd += e_d[k] * Phi(k, q);
                }
                row_a(q) = pa / rho;
                row_d(q) = pd / rho;
            }
            for (int q = 0; q < n_emp; ++q) {
                double pa = 0.0, pd = 0.0;
                for (int k = 0; k < 3; ++k) {
                    pa += e_a[k] * Sens(k, q);
                    pd += e_d[k] * Sens(k, q);
                }
                row_a(6 + q) = pa / rho;
                row_d(6 + q) = pd / rho;
            }

            AtA += w * w * (row_a * row_a.transpose() +
                            row_d * row_d.transpose());
            AtL += w * w * (row_a * d_ra + row_d * d_dec);
            sum_sq += d_ra * d_ra + d_dec * d_dec;
            n += 2;
        }

        if (n == 0) {
            return 1.0E30;
        }

        if (apriori_pos > 0.0) {
            Eigen::Matrix3d Wpos = Eigen::Matrix3d::Zero();
            if (apriori_is_rtn) {
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

        return std::sqrt(sum_sq / static_cast<double>(n)) / rad_per_arcsec;
    };

    sigma0 = evaluate(x, A_t_A, A_t_L, res_ra, res_dec, res_range, n_used);
    std::cout << "  start           angular RMS " << std::fixed
              << std::setprecision(2) << sigma0 << " arcsec   ("
              << n_used << " rows)" << std::endl;

    double lambda = 1.0E-3;
    int iteration_num = 0;
    bool converged = false;
    for (; iteration_num < max_iterations; ++iteration_num) {
        bool accepted = false;
        Eigen::MatrixXd AtA_try = A_t_A;
        Eigen::VectorXd AtL_try = A_t_L;
        std::vector<double> ra_try = res_ra, dec_try = res_dec,
                            rng_try = res_range;
        size_t n_try = n_used;
        double rms_try = sigma0;
        Eigen::VectorXd x_try = x;
        Eigen::VectorXd dx = Eigen::VectorXd::Zero(n_par);

        for (int back = 0; back < 20; ++back) {
            Eigen::MatrixXd damped = A_t_A;
            for (int k = 0; k < n_par; ++k) {
                damped(k, k) += lambda * std::abs(A_t_A(k, k));
            }
            double ignored = 0.0;
            dx = scaled_solve(damped, A_t_L, ignored);
            x_try = x + dx;
            rms_try = evaluate(x_try, AtA_try, AtL_try, ra_try, dec_try,
                               rng_try, n_try);
            if (rms_try < sigma0) {
                accepted = true;
                lambda = std::max(lambda / 3.0, 1.0E-12);
                break;
            }
            lambda *= 10.0;
        }

        if (!accepted) {
            converged = true;
            break;
        }

        x = x_try;
        A_t_A = AtA_try;
        A_t_L = AtL_try;
        res_ra = ra_try;
        res_dec = dec_try;
        res_range = rng_try;
        n_used = n_try;

        std::cout << "  iteration " << std::setw(3) << iteration_num
                  << "   angular RMS " << std::fixed << std::setprecision(2)
                  << rms_try << " arcsec   lambda " << std::scientific
                  << lambda << std::fixed << std::endl;

        const bool small =
            std::abs(dx(0)) < tol_position && std::abs(dx(1)) < tol_position &&
            std::abs(dx(2)) < tol_position && std::abs(dx(3)) < tol_velocity &&
            std::abs(dx(4)) < tol_velocity && std::abs(dx(5)) < tol_velocity &&
            (!estimate_srp || std::abs(dx(6)) < 1.0E-9);
        const bool flat = (sigma0 - rms_try) < 1.0E-5 * rms_try;
        sigma0 = rms_try;
        if (small || flat) {
            converged = true;
            break;
        }
    }

    if (!converged) {
        std::cerr << "\nWarning: the fit did not converge in " << max_iterations
                  << " iterations." << std::endl;
    }

    std::cout << "\n--- fit to " << used.size() << " optical angles ("
              << n_used << " rows) ---\n"
              << "  angular residual RMS : " << std::fixed
              << std::setprecision(2) << sigma0 << " arcsec\n";

    double max_ang = 0.0, mean_rho = 0.0;
    for (size_t i = 0; i < used.size(); ++i) {
        const double a = std::sqrt(res_ra[i] * res_ra[i] +
                                   res_dec[i] * res_dec[i]) / rad_per_arcsec;
        max_ang = std::max(max_ang, a);
        mean_rho += res_range[i];
    }
    mean_rho /= static_cast<double>(used.size());
    std::cout << "  largest              : " << max_ang << " arcsec\n"
              << "  across the sight line: "
              << (sigma0 * rad_per_arcsec * mean_rho * 1000.0)
              << " m at the mean range of " << std::setprecision(1) << mean_rho
              << " km\n";

    std::cout << std::setprecision(2)
              << "\n  epoch                              range km   d(RA)cos"
                 "(dec)   d(dec)   [arcsec]\n";
    for (size_t i = 0; i < used.size(); ++i) {
        std::cout << "  " << used[i].epoch.str_UTC_datestamp() << " "
                  << std::setw(9) << std::setprecision(1) << res_range[i]
                  << "   " << std::setw(10) << std::setprecision(2)
                  << (res_ra[i] / rad_per_arcsec) << "  " << std::setw(9)
                  << (res_dec[i] / rad_per_arcsec) << "\n";
    }

    double cond = 0.0;
    Eigen::MatrixXd unit(n_par, n_par);
    {
        Eigen::VectorXd d(n_par);
        for (int k = 0; k < n_par; ++k) {
            const double a = std::abs(A_t_A(k, k));
            d(k) = (a > 0.0) ? 1.0 / std::sqrt(a) : 1.0;
        }
        const Eigen::MatrixXd As = d.asDiagonal() * A_t_A * d.asDiagonal();
        Eigen::JacobiSVD<Eigen::MatrixXd> svd(
            As, Eigen::ComputeThinU | Eigen::ComputeThinV);
        cond = svd.singularValues()(0) /
               svd.singularValues()(svd.singularValues().size() - 1);
        unit = d.asDiagonal() *
               As.completeOrthogonalDecomposition().pseudoInverse() *
               d.asDiagonal();
    }

    if (estimate_srp) {
        // The weights are already 1/sigma^2 in radians, so the inverse normal
        // matrix is the covariance; scaling it by the residual as well would
        // double-count. Use the ratio instead, which is the usual variance of
        // unit weight.
        const double unit_var = sigma0 * rad_per_arcsec /
                                (sigma_default * rad_per_arcsec);
        const Eigen::MatrixXd cov = unit_var * unit_var * unit;
        const double scale = x(6);
        const double sigma_scale = std::sqrt(std::abs(cov(6, 6)));
        const double acrm = scale * config_ops.area *
                            propagator->rso.get_srp_CR() / config_ops.mass;
        std::cout << std::setprecision(6)
                  << "\n  solar radiation pressure scale : " << scale << " +/- "
                  << sigma_scale << "\n"
                  << "  effective A*C_R/m              : " << acrm << " +/- "
                  << (std::abs(scale) > 0.0
                          ? std::abs(acrm) * sigma_scale / std::abs(scale)
                          : 0.0)
                  << " m^2/kg\n";
    }

    std::cout << "  normal matrix condition number : " << std::scientific
              << cond << std::fixed << "   (after scaling)\n";
    if (cond > 1.0E10) {
        std::cout << "  The geometry does not determine all " << n_par
                  << " parameters; the sigma above is a lower bound.\n";
    }

    std::cout << "\n  An angle says nothing about range. What is fitted here is\n"
                 "  the shape of a track across the sky, so the answer rests on\n"
                 "  the arc being long enough for the geometry to turn, and on\n"
                 "  the prior for whatever it does not.\n";

    return converged ? 0 : 2;
}
