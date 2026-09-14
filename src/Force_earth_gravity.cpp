/*! @file Force_earth_gravity.cpp
	@author David Harrison
	@date 16 May 2016
	@brief SGNL OPS implementation of Earth gravity.
 */

#include "../include/Force_earth_gravity.h"

void Force_earth_gravity::setup(const Resident_constants &rso_const,
                                std::shared_ptr<Resident_variables> in_state)
{
    state = in_state;

    degree = rso_const.grav_degree;
    order = rso_const.grav_order;
    order = (order > degree) ? degree : order;

    if (order > degree) {
        order = degree;
    }

    std::string filepath = "../res/gravity_fields/";

    std::vector<std::string> filenames;
    filenames.emplace_back("none"); // Reserved for custom binary file

    filenames.emplace_back("EGM96.txt");
    filenames.emplace_back("grim5-S1.dat");
    filenames.emplace_back("JGM-3.dat");
    filenames.emplace_back("grim5-C1.dat");
    filenames.emplace_back("GGM01C.GEO");
    filenames.emplace_back("GGM01S.GEO");
    filenames.emplace_back("GGM03C.GEO");
    filenames.emplace_back("EGM2008.txt");

    constexpr int grav_default = 8; // EGM2008
    int grav_num = rso_const.gravity_model;

    if (grav_num < 0 || grav_num >= static_cast<int>(filenames.size())) {
        grav_num = grav_default;
    }

    std::string filename = filenames[static_cast<size_t>(grav_num)];

    std::ifstream infile(filepath + filename);

    std::stringstream report;

    if (!infile.good()) {
        report << "Force_earth_gravity: Could not open gravity file."
               << std::endl
               << "   Gravity Model: " << grav_num << std::endl
               << "            File: " << filepath << filename;
        state->errors.push_back(report.str());

        degree = 0u;
        order = 0u;

        // Since we can't read in the coefficients, default to monopole gravity
        CS.resize(1u);
        CS[0].C = 1.0;
        CS[0].S = 0.0;

        double Re = sgnlOPS::Re;
        GM = sgnlOPS::GM;

        state->set_earth_radius(Re);

        CS = denorm_coef(CS);
    } else {
        report << "   Gravity Model: " << grav_num << ", " << filename;
        state->reports.push_back(report.str());

        // For denormalised C' and S' coefficients
        CS.resize((degree + 1u) * (degree + 2u) / 2u);

        populate_gravity_coefs(grav_num, infile);
    }

    state->set_degree_order(degree, order);

    degree_timevar = 4u; // Desired max time variable gravity degree
    order_timevar = 1u;  // Desired max time variable gravity order

    degree_timevar = (degree_timevar > degree) ? degree : degree_timevar;
    order_timevar =
        (order_timevar > degree_timevar) ? degree_timevar : order_timevar;

    degree_tide = 2u; // Desired max variable gravity degree for tides
    order_tide = 1u;  // Desired max variable gravity order for tides

    degree_tide = (degree_tide > degree) ? degree : degree_tide;
    order_tide = (order_tide > degree_tide) ? degree_tide : order_tide;

    size_t deg_max = std::max(degree_tide, degree_timevar);

    CS_orig.resize((deg_max + 1u) * (deg_max + 2u) / 2u);
    CS_timevar.resize((degree_timevar + 1u) * (degree_timevar + 2u) / 2u);

    // Keep a backup of all coefficients that may be modified
    for (size_t i = 0; i < CS_orig.size(); i++) {
        CS_orig[i] = CS[i];
    }

    // verify_denorm_factors(877);
}

//! Scrape from data files and pre-calculate C' and S' coefficients.
void Force_earth_gravity::populate_gravity_coefs(int grav_num,
                                                 std::ifstream &infile)
{
    std::string dummy;
    size_t n, m;
    double cnm_normalised, snm_normalised;

    double J2dot, J3dot, J4dot; // GRIM5-S1 and C1 rate of change of J2, J3, J4

    std::vector<Spherical_harmonic_coef> norm(CS.size());

    // Set some sensible defaults
    double Re = 6378.1363;
    GM = sgnlOPS::GM;

    norm[0].C = 1.0;

    //************************** EGM96 ******************************

    if (grav_num == 1) {
        // http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm96/egm96.html
        Re = 6378.1363;   // 6378137.0 value on website in m
        GM = 398600.4415; // 0.3986004418D15 value on website in m^3/s^2
        // "The GM and a_e values reported with EGM96 (398600.4415 km^3/s^2 and
        // 6378136.3 m) should be used as scale parameters with the geopotential
        // coefficients.The recommended GM 398600.4418 should be used with the
        // two-body term when working with Geocentric Coordinate Time (TCG)
        // (398600.4415 or 398600.4356 should be used by those still working
        // with Terrestrial Time (TT) or Barycentric Dynamical Time (TDB) units,
        // respectively)."
        // IERS Technical Note 32, Chapter 6

        getline(infile, dummy); // read past the file header

        while (!infile.eof()) {
            infile >> n; // degree of coefficient
            infile >> m; // order of coefficient
            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for EGM96

    //************************** GRIM5-S1 ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	The GRIM5-S1 model has time variable 2,0 3,0 4,0 coefficients,
	starting from 1997. These values must be passed through to the
	orbit integrator to adjust the raw coefficients for their time
	variation.

    GRIM5-S1 documentation states:
	Due to the attenuation of the gravitational signal with altitude,
	not all solved-for coefficients are fully sensed by the satellites.
	The GRIM5-S1 model's resolution therefore can be considered to be
	complete and homogeneous only up to about degree/order 36
	corresponding to a spatial resolution of approximately 1000 km
	(full wavelength) at the Earth surface. The accuracy at this resolution
	is about 50 cm in terms of geoid heights and 3 mgal in terms of surface
	gravity, an improvement by a factor of 1.5 compared to the previous
	solution GRIM4-S4 (J. Geodesy 1997).
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 2) {

        Re = 6378.13646;  // 0.63781364600000E+07 value in file in m
        GM = 398600.4415; // 0.39860044150000E+15 value in file in m^3/s^2

        // read past the file header
        for (int count = 0; count < 6; count++) {
            getline(infile, dummy);
        }

        // skip the first 9 characters
        infile.seekg(9, std::ios::cur);
        infile >> J2dot;
        getline(infile, dummy);

        infile.seekg(9, std::ios::cur);
        infile >> J3dot;
        getline(infile, dummy);

        infile.seekg(9, std::ios::cur);
        infile >> J4dot;
        getline(infile, dummy);

        if (degree_timevar >= 2) {
            CS_timevar[3].C = J2dot;
        }
        if (degree_timevar >= 3) {
            CS_timevar[6].C = J3dot;
        }
        if (degree_timevar >= 4) {
            CS_timevar[10].C = J4dot;
        }

        // Epoch is 1997.0, assuming Julian epoch, this is midday 1st Jan 1997
        epoch_timevar.set_timetag_from_cal_TT(1997, 1, 1, 12, 0, 0.0);

        getline(infile, dummy); // read past the 0,0 line

        // http://www.gfz-potsdam.de/pb1/pg3/grim/grim5_e.html states:
        // C(0,0) may be set to 1.0 without loss of accuracy

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            infile >> n; // degree of coefficient
            infile >> m; // order of coefficient
            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for GRIM5-S1

    //************************** JGM-3 ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	This model has been developed as a joint solution between the
	University of Texas at Austin and NASA/GSFC. It describes the
	gravity field up to degree and order 70.
	Reference:
	B.D. Tapley, M.M. Watkins, J.C. Ries, G. W. Davis, R.J.Eanes,
	S. R.  Poole, H.J. Rim,   B.E. Schutz, C.K. Shum, R.S.Nerem, F.J.
	Lerch, J.A. Marshall, S.M. Klosko,   N.K. Pavlis,
	"The JGM-3 gravity model".

	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 3) {

        Re = 6378.1363;   // 6378136.30 value in file in m
        GM = 398600.4415; // 398600.44150E+09 value in file in m^3/s^2
        // "The GM and a_e values reported with JGM-3 model
        // (398600.4415 km^3/s^2 and 6378136.3 m) should be used as scale
        // parameters with the geopotential coefficients. The recommended GM
        // 398600.4418 should be used with the two-body term when working with
        // SI units (398600.4415 or 398600.4356 should be used by those still
        // working with TDT or TDB units, respectively)."
        // IERS Technical Note 21, Chapter 6

        // read past the file header
        getline(infile, dummy);
        getline(infile, dummy);
        getline(infile, dummy);

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            // get past the first 6 characters
            infile.seekg(6, std::ios::cur);

            infile >> n; // degree of coefficient

            // fix to deal with degree/order field looking like 1010
            if (n > 70) {
                size_t i = n;
                n /= 100;
                m = i - n * 100;
            } else {
                infile >> m; // order of coefficient
            }

            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for JGM-3

    //************************** GRIM5-C1 ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	The GRIM5-C1 model has time variable 2,0 3,0 4,0 coefficients,
	which have been adopted from the GRIM5-S1 model directly
	starting from 1997. These values must be passed through to the
	orbit integrator to adjust the raw coefficients for their time
	variation.

    GRIM5-C1 documentation states:
	The combination of the GRIM5-S1 normal equation system with normal
	equation systems from gridded surface data results in the GRIM5-C1
	global gravity field model. It is computed from a rigorous least squares
	adjustment with a spectral resolution complete to degree and order 120
	corresponding to a spatial resolution of approximately 330 km
	(full wavelength) at the Earth surface. The accuracy at this resolution is
	about 50 cm in terms of geoid heights and 5.5 mgal in terms of surface
	gravity. The following gridded data sets were exploited in the GRIM5-C1
	solution (NIMA - US National Imagery and Mapping Agency, 1995):

		* over continents: NIMA 0.5 x 0.5 deg mean gravity anomalies from
		  terrestrial and airborne gravimetry
		* over shelf areas: NIMA 1 x 1 deg mean gravity anomalies from ship
		  gravimetry
		* over oceans: NIMA 0.5 x 0.5 deg mean gravity anomalies from
		  ocean/sea-ice altimetry gaps (10 %): GRIM5-S1 1 x 1 deg mean gravity
		  anomalies

	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 4) {

        Re = 6378.13646;  // 0.63781364600000E+07 value in file in m
        GM = 398600.4415; // 0.39860044150000E+15 value in file in m^3/s^2

        // read past the file header
        for (int count = 0; count < 6; count++) {
            getline(infile, dummy);
        }

        // skip the first 9 characters
        infile.seekg(9, std::ios::cur);
        infile >> J2dot;
        getline(infile, dummy);

        infile.seekg(9, std::ios::cur);
        infile >> J3dot;
        getline(infile, dummy);

        infile.seekg(9, std::ios::cur);
        infile >> J4dot;
        getline(infile, dummy);

        // Epoch is 1997.0, assuming Julian epoch, this is midday 1st Jan 1997
        epoch_timevar.set_timetag_from_cal_TT(1997, 1, 1, 12, 0, 0.0);

        if (degree_timevar >= 2) {
            CS_timevar[3].C = J2dot;
        }
        if (degree_timevar >= 3) {
            CS_timevar[6].C = J3dot;
        }
        if (degree_timevar >= 4) {
            CS_timevar[10].C = J4dot;
        }

        getline(infile, dummy); // read past the 0,0 line

        // http://www.gfz-potsdam.de/pb1/pg3/grim/grim5_e.html states:
        // C(0,0) may be set to 1.0 without loss of accuracy

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            infile >> n; // degree of coefficient

            // fix to deal with degree/order field looking like 100100
            if (n > 120) {
                size_t i = n;
                n /= 1000;
                m = i - n * 1000;
            } else {
                infile >> m; // order of coefficient
            }

            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for GRIM5-C1

    //************************** GRACE-GGMO1C ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 5) {
        // Notes: ftp://ftp.csr.utexas.edu/pub/grace/GGM01/GGM01_Notes.pdf
        // C20 is a zero-tide value, i.e. it includes the zero-frequency
        // (permanent) tide contribution; to convert to a tide-free system, add
        // 4.173x10-9. Its epoch is 2000, and a rate of
        // C20_dot = +1.162755x10-11/year (J2_dot = -26x10-12/year) was employed

        if (degree_timevar >= 2) {
            CS_timevar[3].C = 1.162755E-11;
            if (order_timevar >= 1) {
                CS_timevar[4].C = -0.337E-11;
                CS_timevar[4].S = 1.606E-11;
            }
        }
        // J2000 as the epoch
        epoch_timevar.set_timetag_from_cal_TT(2000, 1, 1, 12, 0, 0.0);

        Re = 6378.1363;   // 6378136.30 value in file in m
        GM = 398600.4415; // 398600.44150E+09 value in file in m^3/s^2

        // read past the file header
        getline(infile, dummy);
        getline(infile, dummy);
        getline(infile, dummy);

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            // get past the first 6 characters
            infile.seekg(6, std::ios::cur);

            infile >> n; // degree of coefficient

            // fix to deal with degree/order field looking like 100100
            if (n > 200) {
                size_t i = n;
                n /= 1000;
                m = i - n * 1000;
            } else {
                infile >> m; // order of coefficient
            }

            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for GRACE-GGMO1C

    //************************** GRACE-GGMO1S ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 6) {
        // Notes: ftp://ftp.csr.utexas.edu/pub/grace/GGM01/GGM01_Notes.pdf
        // C20 is a zero-tide value, i.e. it includes the zero-frequency
        // (permanent) tide contribution; to convert to a tide-free system, add
        // 4.173x10-9. Its epoch is 2000, and a rate of
        // C20_dot = +1.162755x10-11/year (J2_dot = -26x10-12/year) was employed

        if (degree_timevar >= 2) {
            CS_timevar[3].C = 1.162755E-11;
            if (order_timevar >= 1) {
                CS_timevar[4].C = -0.337E-11;
                CS_timevar[4].S = 1.606E-11;
            }
        }
        // J2000 as the epoch
        epoch_timevar.set_timetag_from_cal_TT(2000, 1, 1, 12, 0, 0.0);

        Re = 6378.1363;   // 6378136.30 value in file in m
        GM = 398600.4415; // 398600.44150E+09 value in file in m^3/s^2

        // read past the file header
        getline(infile, dummy);
        getline(infile, dummy);
        getline(infile, dummy);

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            // GGM01 has dummy line at end with only two zeroes in it...
            if (infile.peek() == 'R') {
                // get past the first 6 characters
                infile.seekg(6, std::ios::cur);

                infile >> n; //degree of coefficient

                // fix to deal with degree/order looking like 200100
                if (n > 200) {
                    size_t i = n;
                    n /= 1000;
                    m = i - n * 1000;
                } else {
                    infile >> m; // order of coefficient
                }

                infile >> cnm_normalised;
                infile >> snm_normalised;

                if ((n <= degree) && (m <= order)) {
                    norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                    norm[n * (n + 1) / 2 + m].S = snm_normalised;
                }
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for GRACE-GGMO1S

    //************************** GRACE-GGMO3C ******************************

    /* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
	* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

    if (grav_num == 7) {
        // Notes from: ftp://ftp.csr.utexas.edu/pub/grace/GGM03/GGM03_Notes.pdf
        // C20 is a zero-tide value, i.e. it includes the zero-frequency
        // (permanent) tide contribution; in order to convert to a tide-free
        // system, add 4.173x10-9. Its epoch is 2005.0, the  approximate
        // mid-point of the four years used (2003 - 2007) in the solution.
        //
        // C21 and S21 were estimated; they were not fixed to the IERS2003
        // standard values. They are epoch 2005 values.

        Re = 6378.1363;   // 6378136.30 value in file in m
        GM = 398600.4415; // 398600.44150E+09 value in file in m^3/s^2

        // read past the file header
        getline(infile, dummy);
        getline(infile, dummy);
        getline(infile, dummy);

        // next line - the normalised gravity field coefficients start
        while (!infile.eof()) {
            // get past the first 6 characters
            infile.seekg(6, std::ios::cur);

            infile >> n; // degree of coefficient

            // fix to deal with degree/order looking like 360100
            if (n > 360) {
                size_t i = n;
                n /= 1000;
                m = i - n * 1000;
            } else {
                infile >> m; // order of coefficient
            }

            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for GRACE-GGMO3C

    // tide free model, we need to add tide corrections to the coefficients
    // http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm2008/egm08_wgs84.html
    // http://earth-info.nga.mil/GandG/wgs84/gravitymod/egm2008/hsynth_WGS84.f
    if (grav_num == 8) {
        Re = 6378.1363;   // 6378137.0 value on website in m
        GM = 398600.4415; // 3.986004418 x 10^14 m^3/s^2 value on website

        while (!infile.eof()) {
            infile >> n; // degree of coefficient
            infile >> m; // order of coefficient
            infile >> cnm_normalised;
            infile >> snm_normalised;

            if ((n <= degree) && (m <= order)) {
                norm[n * (n + 1) / 2 + m].C = cnm_normalised;
                norm[n * (n + 1) / 2 + m].S = snm_normalised;
            }

            getline(infile, dummy); // skip the rest of the line

        } // end of loop reading over gravity field coefficients in file

    } // end of file reading for EGM2008

    state->set_earth_radius(Re);

    CS = denorm_coef(norm); // Perform "enhanced" denormalisation
    CS_timevar = denorm_coef(CS_timevar);
} // End of function populate_gravity_coefs

double Force_earth_gravity::get_GM()
{
    return GM;
}

/**
 * This function uses recursive relations to calculate the "enhanced"
 * denormalisation factors necessary for this model.
 *
 * Can be verified to produce correct output by verify_denorm_factors, which
 * uses an independent method to calculate them.
 *
 * David Harrison
 * 22 March 2015
 *
 * Update: 18 April 2016, this function now calculates g^2 * 4^m / 5^n which
 * does not include sqrt's in the recursive calculation and the function does
 * not blow up as fast. It then carefully divides out the factor and squareroots
 * to get g.
*/
std::vector<std::vector<double>>
Force_earth_gravity::generate_denorm_factors(size_t max_n, size_t max_m) const
{

    size_t n, m;
    double n_d, m_d;

    std::vector<std::vector<double>> g(max_n + 1,
                                       std::vector<double>(max_m + 1));

    n_d = 0.0;
    m_d = 0.0;

    g[0][0] = 0.25;

    if (max_n > 0 && max_m > 0) {
        g[1][1] = 0.6;
    }

    for (m = 0; m <= max_m; m++) {
        if (m > 1) {
            g[m][m] = ((8.0 * m_d + 4.0) * g[m - 1][m - 1]) / (10.0 * m_d);
        }

        n_d = m_d + 1.0;
        for (n = m + 1; n <= max_n; n++) {
            g[n][m] = (2.0 * n_d - 1.0) * (2.0 * n_d + 1.0) * g[n - 1][m] /
                      (5.0 * (n_d - m_d) * (n_d + m_d));
            n_d += 1.0;
        }
        m_d += 1.0;
    }

    double n_corr = 1.0;
    double m_corr = 1.0;
    size_t mod4;

    double n_mult[] = {5.0, 1.0, 1.0, 1.0};
    double g_mult[] = {1.0, 5.0, 25.0, 125.0};

    for (m = 0; m <= max_m; m++) {

        g[m][m] = std::sqrt(g[m][m] * m_corr);

        n_corr = 1.0;

        for (n = m + 1; n <= max_n; n++) {

            mod4 = (n - m) & 3;

            n_corr *= n_mult[mod4];

            g[n][m] =
                (std::sqrt(g[n][m] * g_mult[mod4] * m_corr) * n_corr) * n_corr;
        }

        m_corr *= 1.25;
    }

    return g;

} // End of function generate_denorm_factors

/**
 * This method uses a slower non-recursive method to calculate the
 * denormalisation factors directly, then it compares them to those generated by
 * generate_denorm_factors and outputs the largest error detected.
 *
 * At the time of writing, the output when run to degree and order 877 is:
 * Largest relative error is: 5.329070518200751e-15 for n = 860, m = 269
 * Recursive denorm factor: 1.167605093099534e+240
 *    Direct denorm factor: 1.167605093099528e+240
 *
 * Due to the large factorials used in this, it can only verify up to degree and
 * order 877.
 *
 * David Harrison
 * 22 March 2015
*/
void Force_earth_gravity::verify_denorm_factors(size_t max_n) const
{
    size_t lim = 877;
    max_n = std::min(max_n, lim); // Above this the second method fails

    size_t n, m, worst_n, worst_m;
    long double nfact, nnminus1_prod, nminusm_fact, nplusm_fact;
    double max_err;

    std::vector<std::vector<double>> g = generate_denorm_factors(max_n, max_n),
                                     g_direct(max_n + 1,
                                              std::vector<double>(max_n + 1)),
                                     rel(max_n + 1,
                                         std::vector<double>(max_n + 1));

    nfact = 1.0L;
    nnminus1_prod = 1.0L;

    for (n = 0; n <= max_n; ++n) {

        nplusm_fact = nfact;

        if (n > 1) {
            nfact *= n;
            nnminus1_prod *= n + n - 1;
        }

        nminusm_fact = nfact;

        for (m = 0; m <= n; ++m) {

            if (n == m) {
                nminusm_fact = 1.0L;
            } else if (m > 0) {
                nminusm_fact /= n - m + 1;
            }

            if (n > 0) {
                nplusm_fact *= n + m;
            }

            // std::cout << "(" << n << "-" << m << ")! = " << nminusm_fact
            //           << std::endl;
            // std::cout << "(" << n << "+" << m << ")! = " << nplusm_fact
            //           << std::endl;
            // std::cout << "k = 1 to " << n << " PI (2k-1) = " << nnminus1_prod
            //           << std::endl;

            if (m > 0) {
                g_direct[n][m] = static_cast<double>(
                    std::sqrt(2 * (2 * n + 1) / (nminusm_fact * nplusm_fact)) *
                    nnminus1_prod / 2);
            } else {
                g_direct[n][m] = static_cast<double>(
                    std::sqrt((2 * n + 1) / (nminusm_fact * nplusm_fact)) *
                    nnminus1_prod / 2);
            }
        }
    }

    worst_n = 0;
    worst_m = 0;
    max_err = 0.0;

    for (m = 0; m <= max_n; ++m) {
        for (n = m; n <= max_n; ++n) {

            if (g[n][m] > g_direct[n][m]) {
                rel[n][m] = g[n][m] / g_direct[n][m] - 1;
            } else {
                rel[n][m] = g_direct[n][m] / g[n][m] - 1;
            }

            if (rel[n][m] > max_err) {
                max_err = rel[n][m];
                worst_n = n;
                worst_m = m;
            }
        }
    }

    std::cout.precision(16);
    std::cout << "Largest relative error is: " << rel[worst_n][worst_m]
              << " for n = " << worst_n << ", m = " << worst_m << std::endl;
    std::cout << "Recursive denorm factor: " << g[worst_n][worst_m]
              << std::endl;
    std::cout << "   Direct denorm factor: " << g_direct[worst_n][worst_m]
              << std::endl;

} // End of function verify_denorm_factors

//! Populates C and S arrays with enhanced denormalised values (C' and S').
std::vector<Spherical_harmonic_coef> Force_earth_gravity::denorm_coef(
    const std::vector<Spherical_harmonic_coef> &norm)
{
    const double minusGMR2 = -GM / (state->Re * state->Re);

    std::vector<Spherical_harmonic_coef> denorm(norm.size());

    double deg_d = std::floor(std::sqrt(static_cast<double>(2u * norm.size())));
    size_t deg = static_cast<size_t>(deg_d);

    std::vector<std::vector<double>> g = generate_denorm_factors(deg, deg);

    size_t n = 0;
    size_t m = 0;

    for (size_t nm = 0; nm < norm.size(); ++nm) {
        denorm[nm].C =
            static_cast<double>(2 * n + 1) * g[n][m] * norm[nm].C * minusGMR2;
        denorm[nm].S =
            static_cast<double>(2 * n + 1) * g[n][m] * norm[nm].S * minusGMR2;

        m++;
        if (m > n) {
            n++;
            m = 0;
        }
    }

    return denorm;
}

void Force_earth_gravity::update_gravity_coefficients()
{
    std::vector<Spherical_harmonic_coef> new_CS;
    new_CS.resize(CS_orig.size());

    // Earth pole tide only updates 2,1
    // new_CS[4] = solid_earth_pole_tide_1996();

    // Get time since epoch in Julian years
    double T = (state->eci.epoch - epoch_timevar) / (365.25 * 86400.0);

    for (size_t nm = 3; nm < CS_timevar.size(); ++nm) {
        new_CS[nm].C += CS_timevar[nm].C * T;
        new_CS[nm].S += CS_timevar[nm].S * T;
        CS[nm].C = new_CS[nm].C + CS_orig[nm].C;
        CS[nm].S = new_CS[nm].S + CS_orig[nm].S;
    }
}

// IERS Technical Note 21, Conventions 1996, Section 6, pages 46-47
Spherical_harmonic_coef Force_earth_gravity::solid_earth_pole_tide_1996()
{
    Spherical_harmonic_coef delta_21;
    const double minusGMR2 = -GM / (state->Re * state->Re);

    // Denormalisation of CS coefficient
    // (2 * n + 1) * g[n][m] * C_norm[n][m] * minusGMR2;
    const double denorm = 5.0 * std::sqrt(3.75) * 1.348E-15 * minusGMR2;

    // Anelastic Earth model:
    delta_21.C = -denorm * (state->pm.x + 0.0112 * state->pm.y);
    delta_21.S = denorm * (state->pm.y - 0.0112 * state->pm.x);

    return delta_21;
}

// IERS Technical Note 36, Conventions 2010, Section 6.5, page 94 and
// Section 7.1.4, page 115
// Cubic fit valid from 1976 - 2010, linear fit valid from 2010 onwards
Spherical_harmonic_coef Force_earth_gravity::solid_earth_pole_tide_2010()
{
    Timestruct t = state->eci.epoch.get_TT_tag();
    const long int MJDN_t0 = Timetag::calculate_MJDN_from_cal(2000, 1, 1);

    const double T = (static_cast<double>(t.MJDN - MJDN_t0) * 86400.0 +
                      (static_cast<double>(t.SOD) - 43200.0) + t.SOD_frac) /
                     (36525.0 * 86400.0);

    double mean_x, mean_y;

    // Possible rounding error or typo in IERS recommendations
    // mean_y and d(mean_y)/dt do not quite match at T = 10.0
    if (T > 10.0) {
        mean_x = poly(T, 23513.0, 7614.1); // linear polynomial
        mean_y = poly(T, 358891.0, -628.7);
    } else {
        mean_x = poly(T, 55974.0, 1824.3, 184.13, 7.024); // cubic polynomial
        mean_y = poly(T, 346346.0, 1789.6, -107.29, -0.908);
    }

    double m1 = state->pm.x - mean_x;
    double m2 = mean_y - state->pm.y;

    Spherical_harmonic_coef delta_21;
    const double minusGMR2 = -GM / (state->Re * state->Re);

    // Denormalisation of CS coefficient
    // (2 * n + 1) * g[n][m] * C_norm[n][m] * minusGMR2;
    const double denorm = 5.0 * std::sqrt(3.75) * 1.333E-15 * minusGMR2;

    // Anelastic Earth model:
    delta_21.C = -denorm * (m1 + 0.0115 * m2);
    delta_21.S = -denorm * (m2 - 0.0115 * m1);

    return delta_21;
}

double Force_earth_gravity::compute_potential()
{
    return compute_potential(degree);
}

double Force_earth_gravity::compute_potential(size_t max_n)
{
    size_t n, mp1;
    size_t nm; // 1D vector location for n,m

    double pot = 0.0;
    double pot_n = 0.0;

    n = std::min(max_n, degree);
    nm = n * (n + 1) / 2 + n;

    // Contributions are summed from smallest to largest to preserve precision
    while (n > 1) {
        // Next loop variable is m plus 1 to avoid it going negative
        for (mp1 = n + 1; mp1 > 0; --mp1) {

            pot_n += CS[nm].C * state->VW[nm].V + CS[nm].S * state->VW[nm].W;

            --nm;
        }
        pot += pot_n / (static_cast<double>(n) + 0.5);
        pot_n = 0.0;
        --n;
    }

    // Lastly the monopole case
    pot_n = CS[0].C * state->VW[0].V + CS[0].S * state->VW[0].W;
    pot += pot_n * 2.0;

    // Correct for some extra constants bundled into CS coefficients for
    // acceleration computation
    pot *= -state->Re;

    return pot;
}

double Force_earth_gravity::compute_potential_degree_only(size_t in_n)
{
    size_t n, mp1;
    size_t nm; // 1D vector location for n,m

    double pot = 0.0;

    n = std::min(in_n, degree);
    nm = n * (n + 1) / 2 + n;

    // Contributions are summed from smallest to largest to preserve precision
    if (n > 1u) {
        // Next loop variable is m plus 1 to avoid it going negative
        for (mp1 = n + 1; mp1 > 0; --mp1) {

            pot += CS[nm].C * state->VW[nm].V + CS[nm].S * state->VW[nm].W;

            --nm;
        }
        pot /= (static_cast<double>(n) + 0.5);
    } else if (n == 0u) {
        // Monopole case
        pot = CS[0].C * state->VW[0].V + CS[0].S * state->VW[0].W;
        pot *= 2.0;
    }

    // Correct for some extra constants bundled into CS coefficients for
    // acceleration computation
    pot *= -state->Re;

    return pot;
}

/* Function to calculate Pa/Pr for gravity force model
 * must be called after the calculation of acceleration
 * Ref: Montenbruck 2005, Satellite Orbites Page 245
 * According to David's method, notation for David is C'nm, S'nm, V'nm, W'nm
 * while Cnm, Snm, Vnm and Wnm for Montenbruck's
 * the relations between them are:
 *  Cnm = -C'nm*2*(GM/R^2)/((2n+1)*fnm) ; Snm = -S'nm*2*(GM/R^2)/((2n+1)*fnm)
 *  Vnm = V'nm*fnm ;  Wnm = W'nm*fnm
 *  The indexing method is for C,S,V,W, Vnm = V[n*(n+1)/2 + m];
 */
void Force_earth_gravity::compute_partial_derivatives() const
{
    size_t n, m;

    double paxpx = 0.0, paxpy = 0.0, paxpz = 0.0, paypz = 0.0, pazpz = 0.0;
    Matrix3x3 dadr;
    dadr << 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0;

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
                    paxpx += CS[nm].C *
                                 (state->VW[n2m + 2].V + state->VW[n2m - 2].V) +
                             CS[nm].S *
                                 (state->VW[n2m + 2].W + state->VW[n2m - 2].W);

                    paxpy += CS[nm].C *
                                 (state->VW[n2m + 2].W - state->VW[n2m - 2].W) -
                             CS[nm].S *
                                 (state->VW[n2m + 2].V - state->VW[n2m - 2].V);
                } else {
                    paxpx +=
                        CS[nm].C * (state->VW[n2m + 2].V - state->VW[n2m].V) +
                        CS[nm].S * (state->VW[n2m + 2].W + state->VW[n2m].W);

                    paxpy +=
                        CS[nm].C * (state->VW[n2m + 2].W - state->VW[n2m].W) -
                        CS[nm].S * (state->VW[n2m + 2].V + state->VW[n2m].V);
                }

                paxpz +=
                    CS[nm].C * (state->VW[n2m + 1].V - state->VW[n2m - 1].V) +
                    CS[nm].S * (state->VW[n2m + 1].W - state->VW[n2m - 1].W);

                paypz +=
                    CS[nm].C * (state->VW[n2m + 1].W + state->VW[n2m - 1].W) -
                    CS[nm].S * (state->VW[n2m + 1].V + state->VW[n2m - 1].V);

                pazpz +=
                    CS[nm].C * state->VW[n2m].V + CS[nm].S * state->VW[n2m].W;
            } else {
                paxpx *= 0.5;
                paxpy *= 0.5;

                paxpx += CS[nm].C * state->VW[n2m + 2].V;

                paxpy += CS[nm].C * state->VW[n2m + 2].W;

                paxpz += 2.0 * CS[nm].C * state->VW[n2m + 1].V;

                paypz += 2.0 * CS[nm].C * state->VW[n2m + 1].W;

                pazpz += CS[nm].C * state->VW[n2m].V;

                break;
            }
        }

        dadr(0, 0) += paxpx * minus_2np3;
        dadr(0, 1) += paxpy * minus_2np3;
        dadr(0, 2) += paxpz * minus_2np3;
        dadr(1, 2) += paypz * minus_2np3;
        dadr(2, 2) += pazpz * minus_2np3;

        if (n == 0) {
            break;
        }

        paxpx = 0.0;
        paxpy = 0.0;
        paxpz = 0.0;
        paypz = 0.0;
        pazpz = 0.0;

        minus_2np3 += 2.0;
        n2m -= 2;
    }

    dadr(0, 0) -= dadr(2, 2);

    dadr(0, 0) /= state->Re;
    dadr(0, 1) /= state->Re;
    dadr(0, 2) /= state->Re;
    dadr(1, 2) /= state->Re;
    dadr(2, 2) /= 0.5 * state->Re;

    dadr(1, 0) = dadr(0, 1);
    dadr(1, 1) = -dadr(0, 0) - dadr(2, 2);

    dadr(2, 0) = dadr(0, 2);
    dadr(2, 1) = dadr(1, 2);

    // dadr.print();
    // std::cout << "-------------------------------" << std::endl;

    // Add the ECEF partial matrix to the total ECEF matrix for all forces
    state->dadr_ecef.noalias() += dadr;
}

Cartesian Force_earth_gravity::get_a(size_t n, size_t m) const
{
    Cartesian a;

    if (n > 1 && m <= n && n <= degree && m <= order) {

        size_t nm = n * (n + 1) / 2 + m;
        size_t n1m1 = (n + 1) * (n + 2) / 2 + m + 1;

        if (m > 0) {
            a.x = CS[nm].C * (state->VW[n1m1].V - state->VW[n1m1 - 2].V) +
                  CS[nm].S * (state->VW[n1m1].W - state->VW[n1m1 - 2].W);

            a.y = CS[nm].C * (state->VW[n1m1].W + state->VW[n1m1 - 2].W) -
                  CS[nm].S * (state->VW[n1m1].V + state->VW[n1m1 - 2].V);
        } else {
            a.x = 2.0 * CS[nm].C * state->VW[n1m1].V;
            a.y = 2.0 * CS[nm].C * state->VW[n1m1].W;
        }

        --n1m1;

        a.z =
            2.0 * (CS[nm].C * state->VW[n1m1].V + CS[nm].S * state->VW[n1m1].W);

    } else if (n == 0 && m == 0) {
        a.x = 2.0 * CS[0].C * state->VW[2].V;
        a.y = 2.0 * CS[0].C * state->VW[2].W;
        a.z = 2.0 * CS[0].C * state->VW[1].V;
    }

    return a;
}

void Force_earth_gravity::compute_acceleration()
{
    size_t n, m;
    size_t nm, n1m1; // 1D vector locations for n,m and n+1,m+1

    compute_potential();

    // Condition 1 setup, absolute acceleration value
    // double thresh = 1E-15;

    // Condition 2 setup, making any change to position
    // double thresh0 = std::abs(state->ecef.x * 0x1.0p-51 / 100.0);
    // double thresh1 = std::abs(state->ecef.y * 0x1.0p-51 / 100.0);
    // double thresh2 = std::abs(state->ecef.z * 0x1.0p-51 / 100.0);

    // Enable to use pole tide and time variable gravity
    // update_gravity_coefficients();

    a_ecef.set(0.0, 0.0, 0.0);

    n = degree;

    nm = n * (n + 1) / 2 + n;
    n1m1 = (n + 1) * (n + 2) / 2 + n + 1;

    // Contributions are summed from smallest to largest to preserve precision
    for (n = degree; n > 1; --n) {
        for (m = n; m > 0; --m) {

            a_ecef.x += CS[nm].C * (state->VW[n1m1].V - state->VW[n1m1 - 2].V) +
                        CS[nm].S * (state->VW[n1m1].W - state->VW[n1m1 - 2].W);

            a_ecef.y += CS[nm].C * (state->VW[n1m1].W + state->VW[n1m1 - 2].W) -
                        CS[nm].S * (state->VW[n1m1].V + state->VW[n1m1 - 2].V);

            --n1m1;

            a_ecef.z +=
                CS[nm].C * state->VW[n1m1].V + CS[nm].S * state->VW[n1m1].W;

            --nm;
        }

        double twice_cn0 = CS[nm].C + CS[nm].C; // Cheaper than multiplying by 2

        a_ecef.x += (twice_cn0 * state->VW[n1m1].V);

        a_ecef.y += (twice_cn0 * state->VW[n1m1].W);

        --n1m1;

        a_ecef.z += CS[nm].C * state->VW[n1m1].V;

        --nm;
        --n1m1;

        // Cartesian test_a = a_ecef;
        // test_a.z *= 2.0;

        // Condition 1 evaluation code
        // double acc = test_a.length();
        // if (n > 9 && acc > thresh) {
        //     std::cout << "Degree: " << n << ", Acceleration: " << acc
        //               << " km/s^2 > " << thresh << " km/s^2" << std::endl;
        // }

        // Condition 2 evaluation code
        // if (n > 12 &&
        //     (std::abs(test_a.x) > thresh0 || std::abs(test_a.y) > thresh1 ||
        //      std::abs(test_a.z) > thresh2)) {
        //     std::cout << "Degree: " << n << std::endl;
        //     std::cout << "Acceleration: " << std::abs(test_a.x) << ", "
        //               << std::abs(test_a.y) << ", " << std::abs(test_a.z)
        //               << std::endl;
        //     std::cout << "Thresh-hold: " << thresh0 << ", " << thresh1
        //               << ", " << thresh2 << std::endl
        //               << std::endl;
        // }
    }

    a_ecef.z += a_ecef.z; // Cheaper than multiplying by 2

    // Sum accelerations before computing monopole term for precision reasons
    state->total_a_ecef += a_ecef;

    Cartesian a_mono(state->VW[2].V, state->VW[2].W, state->VW[1].V);
    a_mono *= (CS[0].C + CS[0].C); // Cheaper than multiplying by 2

    // Now add the monopole term in last
    a_ecef += a_mono;
    state->total_a_ecef += a_mono;
}

Cartesian Force_earth_gravity::compute_fast_acceleration(State_vector eci) const
{
    long double r2 = eci.x * eci.x + eci.y * eci.y + eci.z * eci.z;
    long double r = std::sqrt(r2);

    double Re2r2 = static_cast<double>(state->Re_ld * state->Re_ld / r2);

    Cartesian pos = state->frame_transform.rotate_eci_to_ecef(
        Cartesian(eci.x, eci.y, eci.z, r));

    Cartesian a = CS[0].C * pos;

    if (degree > 1u) {
        double z2 = pos.z * pos.z;
        double z30 = z2 - 0.6;
        double z31 = z2 - 0.2;

        a += CS[3].C * Re2r2 * Cartesian(z31 * pos.x, z31 * pos.y, z30 * pos.z);
    }

    return 2.0 * Re2r2 * state->frame_transform.rotate_ecef_to_eci(a);
}
