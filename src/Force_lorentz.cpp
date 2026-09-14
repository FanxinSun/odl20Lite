/*! @file Force_lorentz.cpp
	@author David Harrison
	@date 15 November 2016
	@brief SGNL OPS implementation of the Lorentz force.
 */

#include "../include/Force_lorentz.h"

void Force_lorentz::setup(const Resident_constants &rso_const,
                          std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    degree = rso_const.mag_degree;
    order = rso_const.mag_order;

    if (order > degree) {
        order = degree;
    }

    // Convert specific charge from nC/kg to GC/kg, so that end result gives
    // acceleration in km/s^2
    specific_charge = rso_const.specific_charge * 1.0E-18;

    std::string filepath = "../res/";

    std::vector<std::string> filenames;
    filenames.emplace_back("none"); // Should never be 0, 0 means no field

    filenames.emplace_back("igrf11coeffs.txt");
    filenames.emplace_back("igrf12coeffs.txt");

    constexpr int mag_default = 2; // IGRF12
    int mag_num = rso_const.magnetic_model;

    // But if a 0 gets through, at least do something useful
    if (mag_num < 1 || mag_num >= static_cast<int>(filenames.size())) {
        mag_num = mag_default;
    }

    std::string filename = filenames[static_cast<size_t>(mag_num)];

    std::ifstream infile(filepath + filename);

    std::stringstream report;

    if (!infile.good()) {
        report << "Force_lorentz: Could not open IGRF file." << std::endl
               << "  Magnetic Model: " << mag_num << std::endl
               << "            File: " << filepath << filename;
        state->errors.push_back(report.str());

        // Setup only monopole term (ie: no field) if we can't read in file
        degree = 0u;
        order = 0u;

        coef.resize(1u);
        coef[0].resize(1u);

        std::vector<std::vector<Spherical_harmonic_coef>> temp_coef(
            2u, std::vector<Spherical_harmonic_coef>(1u));

        denorm_coef(temp_coef);
        reformat_coef(temp_coef);

        MJDs.resize(2u);
        MJDs[0] = Timetag::calculate_MJDN_from_cal(1970, 1, 1);
        MJDs[1] = Timetag::calculate_MJDN_from_cal(1975, 1, 1);
    } else {
        report << "  Magnetic Model: " << mag_num << ", " << filename;
        state->reports.push_back(report.str());

        populate_magnetic_coefs(infile);
    }

    // For denormalised C' and S' coefficients
    CS.resize((degree + 1) * (degree + 2) / 2);

    state->set_degree_order(degree, order);

    // verify_denorm_factors(13);
}

//! Scrape from data files and pre-calculate C' and S' coefficients.
void Force_lorentz::populate_magnetic_coefs(std::ifstream &infile)
{
    std::string dummy;
    char gh;
    size_t n = 0u;
    size_t m = 0u;
    size_t i = 0u;
    size_t z = 0u;
    int base_year = 1970; // Ignore IGRF data before this
    double dump;

    // Read in and store IGRF data

    // Read past the file header
    std::getline(infile, dummy);
    std::getline(infile, dummy);
    std::getline(infile, dummy);

    // Skip the first 7 characters
    infile.seekg(7, std::ios::cur);

    double year;
    size_t skip = 0u;

    infile >> year;
    while (static_cast<int>(year) != base_year) {
        infile >> year;
        skip++;
    }

    int prev_year = static_cast<int>(year);
    size_t cols = 2u;

    infile >> year;
    while (static_cast<int>(year) != prev_year) {
        prev_year = static_cast<int>(year);
        infile >> year;
        cols++;
    }

    std::getline(infile, dummy);

    coef.resize(cols - 1);
    for (z = 0u; z < cols - 1; ++z) {
        coef[z].resize((degree + 1) * (degree + 2) / 2);
    }

    std::vector<std::vector<Spherical_harmonic_coef>> temp_coef(
        cols,
        std::vector<Spherical_harmonic_coef>((degree + 1) * (degree + 2) / 2));

    // Now begin reading and storing data

    while (infile >> gh >> n >> m) {

        if (n <= degree && m <= order) {

            i = n * (n + 1u) / 2u + m;

            for (z = 0u; z < skip; ++z) {
                infile >> dump;
            }

            for (z = 0u; z < cols; ++z) {
                if (gh == 'g') {
                    infile >> temp_coef[z][i].C;
                } else {
                    infile >> temp_coef[z][i].S;
                }
            }

            --z;

            if (gh == 'g') {
                temp_coef[z][i].C *= 5.0;
            } else {
                temp_coef[z][i].S *= 5.0;
            }
        }

        std::getline(infile, dummy);
    }

    denorm_coef(temp_coef);   // Perform "enhanced" denormalisation
    reformat_coef(temp_coef); // Change data structure for better performance

    MJDs.resize(cols);
    for (z = 0; z < cols; ++z) {
        MJDs[z] = Timetag::calculate_MJDN_from_cal(
            base_year + static_cast<int>(z * 5), 1, 1);
    }

} // End of function populate_gravity_coefs

/**
 * This method uses a slower non-recursive method to calculate the
 * denormalisation factors directly, but the IGRF is only 13x13 so this is fine.
 *
 * Populates C and S arrays with enhanced denormalised values (C' and S').
*/
void Force_lorentz::denorm_coef(
    std::vector<std::vector<Spherical_harmonic_coef>> &temp_coef)
{
    size_t n, m, i, z;
    double nfact, nnminus1_prod, nminusm_fact, nplusm_fact;

    std::vector<std::vector<double>> g_direct(degree + 1,
                                              std::vector<double>(degree + 1));

    nfact = 1.0;
    nnminus1_prod = 1.0;

    // The value for Re used in the IGRF model is different to the radius used
    // in the gravity models, so some conversion has to be done. This correction
    // is included in the denormalised magnetic field coefficients.
    double Re_mag = 6371.2;
    double Re_grav = state->Re;

    double Re_corr = Re_mag / Re_grav;
    double Re_nplus2 = Re_mag * Re_mag / (Re_grav * Re_grav);

    double n_d = 0.0;
    double m_d = 0.0;

    for (n = 0; n <= degree; ++n) {

        nplusm_fact = nfact;

        if (n > 1) {
            nfact *= n_d;
            nnminus1_prod *= n_d + n_d - 1.0;
        }

        nminusm_fact = nfact;

        m_d = 0.0;

        for (m = 0; m <= n; ++m) {

            if (n == m) {
                nminusm_fact = 1.0;
            } else if (m > 0) {
                nminusm_fact /= n_d - m_d + 1.0;
            }

            if (n > 0) {
                nplusm_fact *= n_d + m_d;
            }

            if (m > 0) {
                g_direct[n][m] = 0.5 * (2.0 * n_d + 1.0) *
                                 std::sqrt(2.0 / (nminusm_fact * nplusm_fact)) *
                                 nnminus1_prod * Re_nplus2;
            } else {
                g_direct[n][m] = 0.5 * (2.0 * n_d + 1.0) *
                                 std::sqrt(1.0 / (nminusm_fact * nplusm_fact)) *
                                 nnminus1_prod * Re_nplus2;
            }

            i = n * (n + 1u) / 2u + m;

            for (z = 0; z < temp_coef.size(); ++z) {
                temp_coef[z][i].C = temp_coef[z][i].C * g_direct[n][m];
                temp_coef[z][i].S = temp_coef[z][i].S * g_direct[n][m];
            }

            m_d += 1.0;
        }

        Re_nplus2 *= Re_corr;
        n_d += 1.0;
    }

} // End of function denorm_coef

//! Populates C and S arrays with enhanced denormalised values (C' and S').
void Force_lorentz::reformat_coef(
    const std::vector<std::vector<Spherical_harmonic_coef>> &temp_coef)
{
    size_t i, j;
    size_t cols = coef.size();
    size_t length = coef[0].size();

    for (j = 0; j < length; j++) {
        coef[0][j].first = temp_coef[0][j];
    }

    for (i = 0; i < cols - 1; i++) {
        for (j = 0; j < length; j++) {
            coef[i][j].second.C = temp_coef[i + 1][j].C - coef[i][j].first.C;
            coef[i][j].second.S = temp_coef[i + 1][j].S - coef[i][j].first.S;
            coef[i + 1][j].first = temp_coef[i + 1][j];
        }
    }

    for (j = 0; j < length; j++) {
        coef[i][j].second = temp_coef[i + 1][j];
    }

} // End of function reformat_coef

void Force_lorentz::calculate_CS()
{
    size_t z = 0;
    size_t i;
    size_t dates = MJDs.size();
    long int sec_in_day = 0; // Passed by reference and set by Timetag class
    double lerp_frac = 0.0;

    Timestruct now = state->eci.epoch.get_UTC_tag(sec_in_day);

    while (z < dates && now.MJDN > MJDs[z]) {
        z++;
    }

    if (z == dates) {
        z -= 2;
        lerp_frac = 1.0;
    } else if (z == 0) {
        lerp_frac = 0.0;
    } else {
        z--;
        lerp_frac = (static_cast<double>(now.MJDN - MJDs[z]) +
                     (static_cast<double>(now.SOD) + now.SOD_frac) /
                         static_cast<double>(sec_in_day)) /
                    static_cast<double>(MJDs[z + 1] - MJDs[z]);
    }

    for (i = 0; i < CS.size(); ++i) {
        CS[i].C = coef[z][i].first.C + lerp_frac * coef[z][i].second.C;
        CS[i].S = coef[z][i].first.S + lerp_frac * coef[z][i].second.S;
    }
}

/* Function to calculate Pa/Pr and Pa/Pv for magnetic field model
 * must be called after the calculation of acceleration
 * Ref: Montenbruck 2005, Satellite Orbites Page 245
 * According to David's method, notation for David is C'nm, S'nm, V'nm, W'nm
 * while Cnm, Snm, Vnm and Wnm for Montenbruck's
 * the relations between them are:
 *  Cnm = -C'nm*2*(GM/R^2)/((2n+1)*fnm) ; Snm = -S'nm*2*(GM/R^2)/((2n+1)*fnm)
 *  Vnm = V'nm*fnm ;  Wnm = W'nm*fnm
 *  The indexing method is for C,S,V,W, Vnm = V[n*(n+1)/2 + m];
 */
void Force_lorentz::compute_partial_derivatives() const
{
    size_t n, m;

    double pBxpx = 0.0, pBxpy = 0.0, pBxpz = 0.0, pBypz = 0.0, pBzpz = 0.0;
    Matrix3x3 dBdr;
    dBdr << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;

    double minus_2np3 = -static_cast<double>(2 * degree + 3);

    // 1D vector locations for n,m and n+2,m
    size_t nm = degree * (degree + 1) / 2 + degree + 1;
    size_t n2m = (degree + 2) * (degree + 3) / 2 + degree + 1;

    // Sum up all the degree and order
    // Due to use of unsigned ints (size_t), break conditions are at end of loop
    for (n = degree;; --n) {
        for (m = n;; --m) {
            --nm;
            --n2m;

            if (m > 0) {
                if (m > 1) {
                    pBxpx += CS[nm].C *
                                 (state->VW[n2m + 2].V + state->VW[n2m - 2].V) +
                             CS[nm].S *
                                 (state->VW[n2m + 2].W + state->VW[n2m - 2].W);

                    pBxpy += CS[nm].C *
                                 (state->VW[n2m + 2].W - state->VW[n2m - 2].W) -
                             CS[nm].S *
                                 (state->VW[n2m + 2].V - state->VW[n2m - 2].V);
                } else {
                    pBxpx +=
                        CS[nm].C * (state->VW[n2m + 2].V - state->VW[n2m].V) +
                        CS[nm].S * (state->VW[n2m + 2].W + state->VW[n2m].W);

                    pBxpy +=
                        CS[nm].C * (state->VW[n2m + 2].W - state->VW[n2m].W) -
                        CS[nm].S * (state->VW[n2m + 2].V + state->VW[n2m].V);
                }

                pBxpz +=
                    CS[nm].C * (state->VW[n2m + 1].V - state->VW[n2m - 1].V) +
                    CS[nm].S * (state->VW[n2m + 1].W - state->VW[n2m - 1].W);

                pBypz +=
                    CS[nm].C * (state->VW[n2m + 1].W + state->VW[n2m - 1].W) -
                    CS[nm].S * (state->VW[n2m + 1].V + state->VW[n2m - 1].V);

                pBzpz +=
                    CS[nm].C * state->VW[n2m].V + CS[nm].S * state->VW[n2m].W;
            } else {
                pBxpx *= 0.5;
                pBxpy *= 0.5;

                pBxpx += CS[nm].C * state->VW[n2m + 2].V;

                pBxpy += CS[nm].C * state->VW[n2m + 2].W;

                pBxpz += 2.0 * CS[nm].C * state->VW[n2m + 1].V;

                pBypz += 2.0 * CS[nm].C * state->VW[n2m + 1].W;

                pBzpz += CS[nm].C * state->VW[n2m].V;

                break;
            }
        }

        dBdr(0, 0) += pBxpx * minus_2np3;
        dBdr(0, 1) += pBxpy * minus_2np3;
        dBdr(0, 2) += pBxpz * minus_2np3;
        dBdr(1, 2) += pBypz * minus_2np3;
        dBdr(2, 2) += pBzpz * minus_2np3;

        if (n == 1) {
            break;
        }

        pBxpx = 0.0;
        pBxpy = 0.0;
        pBxpz = 0.0;
        pBypz = 0.0;
        pBzpz = 0.0;

        minus_2np3 += 2.0;
        n2m -= 2;
    }

    dBdr(0, 0) -= dBdr(2, 2);

    dBdr(0, 0) /= state->Re;
    dBdr(0, 1) /= state->Re;
    dBdr(0, 2) /= state->Re;
    dBdr(1, 2) /= state->Re;
    dBdr(2, 2) /= 0.5 * state->Re;

    dBdr(1, 0) = dBdr(0, 1);
    dBdr(1, 1) = -dBdr(0, 0) - dBdr(2, 2);

    dBdr(2, 0) = dBdr(0, 2);
    dBdr(2, 1) = dBdr(1, 2);

    // dBdr.print();
    // std::cout << "-------------------------------" << std::endl;

    Cartesian qv = specific_charge * state->ecef_rso_vel;
    Cartesian qB = specific_charge * B;

    Matrix3x3 qv_x, dadv;

    // clang-format off
    qv_x <<   0.0, -qv.z,  qv.y,
             qv.z,   0.0, -qv.x,
            -qv.y,  qv.x,   0.0;

    dadv <<   0.0,  qB.z, -qB.y,
            -qB.z,   0.0,  qB.x,
             qB.y, -qB.x,   0.0;
    // clang-format on

    // Add the ECEF partial matrix to the total ECEF matrix for all forces
    // da/dr = q * d(v X B)/dr = q * [v]_x * dB/dr
    state->dadr_ecef.noalias() += qv_x * dBdr;

    // da/dv = q * d(v X B)/dv = q * I * [B]_x^t = q * [B]_x^t
    state->dadv_ecef.noalias() += dadv;
}

Cartesian Force_lorentz::get_Bnm(size_t n, size_t m) const
{
    Cartesian Bnm;

    if (n > 0 && m <= n && n <= degree && m <= order) {

        size_t nm = n * (n + 1) / 2 + m;
        size_t n1m1 = (n + 1) * (n + 2) / 2 + m + 1;

        if (m > 0) {
            Bnm.x = CS[nm].C * (state->VW[n1m1].V - state->VW[n1m1 - 2].V) +
                    CS[nm].S * (state->VW[n1m1].W - state->VW[n1m1 - 2].W);

            Bnm.y = CS[nm].C * (state->VW[n1m1].W + state->VW[n1m1 - 2].W) -
                    CS[nm].S * (state->VW[n1m1].V + state->VW[n1m1 - 2].V);
        } else {
            Bnm.x = 2.0 * CS[nm].C * state->VW[n1m1].V;
            Bnm.y = 2.0 * CS[nm].C * state->VW[n1m1].W;
        }

        --n1m1;

        Bnm.z =
            2.0 * (CS[nm].C * state->VW[n1m1].V + CS[nm].S * state->VW[n1m1].W);
    }

    return Bnm;
}

void Force_lorentz::calculate_field()
{
    size_t n, m;
    double twice_cn0;

    size_t nm, n1m1; // 1D vector locations for n,m and n+1,m+1

    n = degree;

    nm = n * (n + 1) / 2 + n;
    n1m1 = (n + 1) * (n + 2) / 2 + n + 1;

    B.set(0.0, 0.0, 0.0);

    // Contributions are summed from smallest to largest to preserve precision
    for (n = degree; n > 0; --n) {
        for (m = n; m > 0; --m) {

            B.x += CS[nm].C * (state->VW[n1m1].V - state->VW[n1m1 - 2].V) +
                   CS[nm].S * (state->VW[n1m1].W - state->VW[n1m1 - 2].W);

            B.y += CS[nm].C * (state->VW[n1m1].W + state->VW[n1m1 - 2].W) -
                   CS[nm].S * (state->VW[n1m1].V + state->VW[n1m1 - 2].V);

            --n1m1;

            B.z += CS[nm].C * state->VW[n1m1].V + CS[nm].S * state->VW[n1m1].W;

            --nm;
        }

        twice_cn0 = CS[nm].C + CS[nm].C; // Cheaper than multiplying by 2

        B.x += (twice_cn0 * state->VW[n1m1].V);

        B.y += (twice_cn0 * state->VW[n1m1].W);

        --n1m1;

        B.z += (CS[nm].C * state->VW[n1m1].V + CS[nm].S * state->VW[n1m1].W);

        --nm;
        --n1m1;
    }

    B.z += B.z; // Cheaper than multiplying by 2
}

void Force_lorentz::compute_acceleration()
{
    calculate_CS();
    calculate_field();

    // alternate();
    // output_field();

    a_ecef = specific_charge * cross_product(state->ecef_rso_vel, B);
    state->total_a_ecef += a_ecef;
}

void Force_lorentz::output_field()
{
    double lon, lat;
    lon = std::atan2(state->ecef_rso.y, state->ecef_rso.x) * sgnlOPS::D_RAD2DEG;
    lat = std::asin(state->ecef_rso_hat.z) * sgnlOPS::D_RAD2DEG;

    double sigma = std::sqrt(state->ecef_rso.x * state->ecef_rso.x +
                             state->ecef_rso.y * state->ecef_rso.y);

    double slon = state->ecef_rso.y / sigma;
    double clon = state->ecef_rso.x / sigma;
    double slat = state->ecef_rso_hat.z;
    double clat = sigma / state->r;

    // This rotation matrix and supporting calculations are based on a spherical
    // model of the Earth NOT the typical ellipsoidal geodetic model
    Matrix3x3 ECEF_ENU;
    ECEF_ENU(0, 0) = -slon;
    ECEF_ENU(0, 1) = clon;
    ECEF_ENU(0, 2) = 0.0;

    ECEF_ENU(1, 0) = -clon * slat;
    ECEF_ENU(1, 1) = -slon * slat;
    ECEF_ENU(1, 2) = clat;

    ECEF_ENU(2, 0) = state->ecef_rso_hat.x; // clon * clat
    ECEF_ENU(2, 1) = state->ecef_rso_hat.y; // slon * clat
    ECEF_ENU(2, 2) = slat;

    Cartesian B_ENU = ECEF_ENU * B;

    std::cout << "Position (km) & Time (UTC):" << std::endl
              << "x: " << state->ecef_rso.x << ", y: " << state->ecef_rso.y
              << ", z: " << state->ecef_rso.z << std::endl;
    std::cout << "lat: " << lat << ", lon: " << lon << ", r: " << state->r
              << std::endl;
    state->ecef.epoch.print_UTC_datestamp();

    // Note that the IGRF define downwards to be positive, so for the vertical
    // component of the field, we output -B_ENU.z
    // Source: http://www.geomag.bgs.ac.uk/data_service/models_compass/igrf.html
    std::cout << std::setprecision(16) << "\nField Strength (nT):" << std::endl;
    std::cout << "    x: " << B.x << ",    y: " << B.y << ",    z: " << B.z
              << std::endl
              << "North: " << B_ENU.y << ", East: " << B_ENU.x
              << ", Vert: " << -B_ENU.z << std::endl
              << "Total: " << B.length() << std::endl
              << "--------------------------------------" << std::endl;
}
