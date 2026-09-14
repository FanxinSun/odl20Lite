/*! @file Frame_transform.cpp
	@author Santosh Bhattarai
	@date 1 April 2014
	@brief SGNL OPS implementation file defining functions associated with
		   ecef/eci transformations.

	Functions associated with ecef/eci frame transformations are defined here.
	These include routines that deal with rotations based on Earth rotation
	angle, polar motion, precession and nutation. Nominally, the inertial frame
	that is used in the SGNL OPS software is J2000.
 */

#include "../include/Frame_transform.h"

// clang-format off
std::array<uint16_t, 106> Frame_transform::populate_nutation_arrays()
{
    int orig_mul[106][5] = {
        { 0,  0,  0,  0,  1}, { 0,  0,  2, -2,  2}, { 0,  0,  2,  0,  2},
        { 0,  0,  0,  0,  2}, { 0, -1,  0,  0,  0}, { 0,  1,  2, -2,  2},
        { 1,  0,  2,  0,  2}, { 0, -1,  2, -2,  2}, { 1,  0,  0,  0,  0},
        { 0,  0,  2,  0,  1}, { 0,  0,  2, -2,  1}, { 1,  0,  0,  0,  1},
        {-1,  0,  0,  0,  1}, { 0,  2,  0,  0,  0}, { 0,  2,  2, -2,  2},
        {-1,  0,  0,  2,  0}, {-1,  0,  2,  0,  2}, { 0,  0,  0,  2,  0},
        {-1,  0,  2,  2,  2}, { 1,  0,  2,  0,  1}, {-2,  0,  0,  2,  0},
        {-2,  0,  2,  0,  1}, { 0,  0,  2,  2,  2}, { 2,  0,  2,  0,  2},
        { 2,  0,  0,  0,  0}, { 1,  0,  2, -2,  2}, { 0,  0,  2,  0,  0},
        {-1,  0,  2,  0,  1}, {-1,  0,  0,  2,  1}, { 0,  1,  0,  0,  1},
        { 1,  0,  0, -2,  1}, { 0, -1,  0,  0,  1}, {-1,  0,  2,  2,  1},
        { 1,  0,  2,  2,  2}, { 0, -1,  2,  0,  2}, { 0,  0,  2,  2,  1},
        { 0,  1,  2,  0,  2}, {-2,  0,  0,  2,  1}, { 0,  0,  0,  2,  1},
        { 2,  0,  2, -2,  2}, { 1,  0,  2, -2,  1}, { 0,  0,  0, -2,  1},
        { 0, -1,  2, -2,  1}, { 2,  0,  2,  0,  1}, { 2,  0,  0, -2,  1},
        { 0,  1,  2, -2,  1}, {-1, -1,  2,  2,  2}, { 0, -1,  2,  2,  2},
        { 1, -1,  2,  0,  2}, { 3,  0,  2,  0,  2}, {-2,  0,  2,  0,  2},
        {-1,  0,  2,  4,  2}, { 1,  0,  0,  0,  2}, {-1,  0,  2, -2,  1},
        { 0, -2,  2, -2,  1}, {-2,  0,  0,  0,  1}, { 2,  0,  0,  0,  1},
        { 1,  1,  2,  0,  2}, { 0,  0,  2,  1,  2}, {-1,  0, -2, -2, -1},
        { 2,  0, -2, -4, -2}, { 1,  1,  2, -2,  2}, {-2,  0,  2,  2,  2},
        {-1,  0,  0,  0,  2}, { 2,  0,  2, -2,  1}, { 0,  0,  2, -2,  0},
        { 2,  0, -2,  0,  0}, { 1,  1,  0, -2,  0}, { 1,  0,  0,  2,  0},
        { 1, -1,  0,  0,  0}, { 1,  0,  0, -1,  0}, { 0,  0,  0,  1,  0},
        { 0,  1,  0, -2,  0}, { 1,  0, -2,  0,  0}, { 1,  1,  0,  0,  0},
        { 1, -1,  0, -1,  0}, { 1,  0,  2,  0,  0}, { 3,  0,  0,  0,  0},
        {-1,  0,  0, -2, -1}, {-1, -1,  0,  2, -1}, { 0, -1,  0, -2,  0},
        { 0, -1, -2,  2,  0}, { 0, -1,  2, -2,  0}, {-1,  0,  2, -2,  0},
        {-1,  0,  2,  2,  0}, {-1,  0, -2,  2,  0}, {-1,  0,  0,  4,  0},
        {-2,  0,  0,  4,  0}, { 0,  0, -2, -4, -2}, { 0,  0, -2,  1, -2},
        {-2,  0, -2, -2, -2}, { 0,  1, -2,  0, -1}, { 0,  0,  2,  0, -1},
        { 0,  0,  4, -2,  2}, { 0,  1,  0,  0,  2}, { 3,  0,  2, -2,  2},
        { 0,  0, -2,  2,  1}, { 0,  1,  2,  0,  1}, {-1,  0,  4,  0,  2},
        { 2,  1,  0, -2,  0}, { 2,  0,  0,  2,  0}, { 2,  0, -2,  0,  1},
        { 1, -1,  0, -2,  0}, {-1,  0,  0,  1,  1}, {-1, -1,  0,  2,  1},
        { 0,  1,  0,  1,  0}};

    int a, b, c, d, e;
    std::array<uint16_t, 106> n;

    for (size_t i = 0; i < 106; ++i) {
        a = (orig_mul[i][0] + 2) << 12u;

        b = (orig_mul[i][1] + 2) << 9u;

        c = orig_mul[i][2] + 4;
        if (c == 8) {c = 7;}
        if (c > 0) {c--;}
        c = c << 6u;

        d = orig_mul[i][3] + 4;
        if (d == 8) {d = 7;}
        if (d > 0) {d--;}
        d = d << 3u;

        e = orig_mul[i][4] + 2;

        // Pack all argument multipliers into a single 16 bit integer
        n[i] = static_cast<uint16_t>(a + b + c + d + e);
    }

    return n;
}
// clang-format on

// For IAU 1980 nutation model
const std::array<uint16_t, 106> Frame_transform::n_mul =
    Frame_transform::populate_nutation_arrays();

// ABCD all in micro arc-seconds
// clang-format off
const double Frame_transform::n_BD[15][2] = {
    {-17420.0, 890.0}, {-160.0, -310.0}, {-20.0, -50.0}, { 20.0, 50.0},
    {   340.0, -10.0}, { 120.0,  -60.0}, {  0.0, -10.0}, {-50.0, 30.0},
    {    10.0,   0.0}, { -40.0,    0.0}, { 10.0,   0.0}, { 10.0,  0.0},
    {   -10.0,   0.0}, { -10.0,    0.0}, { 10.0,   0.0}
};

const double Frame_transform::n_AC[78][2] = {
    {-17199600.0, 9202500.0}, { -1318700.0,  573600.0},
    {  -227400.0,   97700.0}, {   206200.0,  -89500.0},
    {  -142600.0,    5400.0}, {   -51700.0,   22400.0},
    {   -30100.0,   12900.0}, {    21700.0,   -9500.0},
    {    71200.0,    -700.0}, {   -38600.0,   20000.0},
    {    12900.0,   -7000.0}, {     6300.0,   -3300.0},
    {    -5800.0,    3200.0}, {     1700.0,       0.0},
    {    -1600.0,     700.0},
    // B & D always = 0 after here
    {15800.0,  -100.0}, {12300.0, -5300.0}, { 6300.0,  -200.0},
    {-5900.0,  2600.0}, {-5100.0,  2700.0}, {-4800.0,   100.0},
    { 4600.0, -2400.0}, {-3800.0,  1600.0}, {-3100.0,  1300.0},
    { 2900.0,  -100.0}, { 2900.0, -1200.0}, { 2600.0,  -100.0},
    { 2100.0, -1000.0}, { 1600.0,  -800.0}, {-1500.0,   900.0},
    {-1300.0,   700.0}, {-1200.0,   600.0}, {-1000.0,   500.0},
    { -800.0,   300.0}, { -700.0,   300.0}, { -700.0,   300.0},
    {  700.0,  -300.0}, { -600.0,   300.0}, { -600.0,   300.0},
    {  600.0,  -300.0}, {  600.0,  -300.0}, { -500.0,   300.0},
    { -500.0,   300.0}, { -500.0,   300.0}, {  400.0,  -200.0},
    {  400.0,  -200.0}, { -300.0,   100.0}, { -300.0,   100.0},
    { -300.0,   100.0}, { -300.0,   100.0}, { -300.0,   100.0},
    { -200.0,   100.0}, { -200.0,   100.0}, { -200.0,   100.0},
    { -200.0,   100.0}, { -200.0,   100.0}, {  200.0,  -100.0},
    {  200.0,  -100.0}, {  200.0,  -100.0}, {  100.0,   100.0},
    {  100.0,   100.0}, {  100.0,  -100.0}, {  100.0,  -100.0},
    {  100.0,  -100.0}, {  100.0,  -100.0},
    // C always = 0 after here
    {-2200.0, 0.0}, { 1100.0, 0.0}, { -700.0, 0.0}, {  600.0, 0.0},
    {  500.0, 0.0}, { -400.0, 0.0}, { -400.0, 0.0}, { -400.0, 0.0},
    {  400.0, 0.0}, { -300.0, 0.0}, { -300.0, 0.0}, {  300.0, 0.0},
    {  200.0, 0.0}
};
// clang-format on

void Frame_transform::setup(Timetag start, double sim_time)
{
    Timetag end = start;
    end.step(sim_time);

    setup(start, end);
}

void Frame_transform::setup(Timetag start, Timetag end)
{
    long int MJD_endday, dummy;

    MJD_startday = start.get_UTC_tag(dummy).MJDN;
    MJD_endday = end.get_UTC_tag(dummy).MJDN;

    if (MJD_startday > MJD_endday) {
        long int temp = MJD_startday;
        MJD_startday = MJD_endday;
        MJD_endday = temp;
    }

    read_in_data(MJD_endday);
}

void Frame_transform::read_in_data(long int MJD_endday)
{
    std::string eop_file = "../res/eopc04";

    if (!populate_IERS_coefficient_tables(eop_file, MJD_endday)) {
        std::cerr << "Quitting!" << std::endl;
        std::exit(1);
    }
}

polar_motion Frame_transform::compute_rotations(Timetag epoch,
                                                double TDB_minus_TT)
{
    Timestruct UTC, TDB;
    double JC_TDB;

    double Xi, Z, Theta; // Precession angles
    double GMST, GAST;   // Greenwich Apparent Sidereal Time

    long int sec_in_UTC_day;
    UTC = epoch.get_UTC_tag(sec_in_UTC_day);
    TDB = epoch.get_TT_tag();
    TDB.SOD_frac += TDB_minus_TT;

    // Get number of Julian centuries since J2000 in TDB
    JC_TDB = ((static_cast<double>(TDB.MJDN) - Timetag::MJD_1_jan_2000_midday) *
                  86400.0 +
              static_cast<double>(TDB.SOD) + TDB.SOD_frac) /
             (86400.0 * 36525.0);

    // Calculate Lunar and Solar Parameters **************************

    lunar_solar_arg ls_arg = calculate_lunar_solar_arguments(JC_TDB);

    // Calculate Earth Orientation Parameters ************************

    IERS_record record = interpolate_IERS_parameters(UTC, sec_in_UTC_day);

    // Compute and add the Ray model tidal corrections to interpolated params
    GMST = RAY(UTC, ls_arg, record);

    // Ray model needed these in arc-seconds, but everything else wants radians
    ls_arg.MMoon *= sgnlOPS::sec_to_rad;
    ls_arg.MSun *= sgnlOPS::sec_to_rad;
    ls_arg.U *= sgnlOPS::sec_to_rad;
    ls_arg.DSun *= sgnlOPS::sec_to_rad;
    ls_arg.omega *= sgnlOPS::sec_to_rad;

    // PRECESSION ****************************************************

    // Compute the precession angles
    Theta = JC_TDB * sgnlOPS::sec_to_rad; // Borrow Theta for a moment

    Xi = poly(JC_TDB, 2306.2181, 0.30188, 0.017998) * Theta;
    Z = poly(JC_TDB, 2306.2181, 1.09468, 0.018203) * Theta;
    Theta *= poly(JC_TDB, 2004.3109, -0.42665, -0.041833);

    // NUTATION ******************************************************

    // Compute the IERS empirical nutation corrections and update the nutations
    // See IERS Technical note 21, page 22
    // This returns nutation arguments in micro arcseconds
    nutation nut =
        compute_nutation(JC_TDB, ls_arg, record.dEpsilon, record.dPsi);

    // GAST **********************************************************

    // This function makes use of the nutation calculations
    // Also converts nutation arguments into radians
    GAST = compute_GAST(UTC.MJDN, GMST, ls_arg.omega, nut);

    // POLAR MOTION **************************************************

    // Polar motion parameters already corrected in RAY model

    // Compute the combined rotation matrix **************************
    auto RW = rotate_around_y(record.pm.x * sgnlOPS::microsec_to_rad);
    RW *= rotate_around_x(record.pm.y * sgnlOPS::microsec_to_rad);

    RW *= rotate_around_z(-GAST);

    auto PN = rotate_around_x(nut.E);
    PN *= rotate_around_z(nut.deltaPhi);
    PN *= rotate_around_x(-nut.Ebar);

    PN *= rotate_around_z(Z);
    PN *= rotate_around_y(-Theta);
    PN *= rotate_around_z(Xi);

    R = get_matrix(RW * PN); // Create ECI to ECEF rotation matrix, R

    // A rotation matrix should have a determinant of exactly 1, and to ensure
    // this we should divide each element by the cube root of the determinant.
    // However since the matrix should already have a determinant extremely
    // close to 1, we make the following approximations:
    // det(R) = 1 + x
    // cbrt(det(R)) ~= 1 + x/3
    // 1/cbrt(det(R)) ~= 1 - x/3
    double norm_factor = 1.0 - (R.determinant() - 1.0) / 3.0;

    R *= norm_factor; // Correct R matrix

    // Correct dR/dt matrix by including the factor in Earth's angular velocity
    double w = record.w * norm_factor;

    // Matrix to compute differential of rotation matrix
    // clang-format off
    Matrix3x3 differential;
    differential << 0.0,   w, 0.0,
                     -w, 0.0, 0.0,
                    0.0, 0.0, 0.0;
    // clang-format on

    V = get_matrix(RW) * differential * get_matrix(PN); // Create dR/dt

    return record.pm; // Return polar motion parameters in micro-arcseconds
} // end of function compute_rotations

// Input argument is time in Julian centuries of barycentric time since J2000.
lunar_solar_arg Frame_transform::calculate_lunar_solar_arguments(double JC_TDB)
{
    lunar_solar_arg ls_arg;

    // Calculate L, Mean anomaly of Moon
    ls_arg.MMoon = poly(JC_TDB, 485868.249036, 1717915923.2178, 31.8792,
                        0.051635, -0.00024470);

    // Calculate MSun, Mean anomaly of Sun
    ls_arg.MSun = poly(JC_TDB, 1287104.793048, 129596581.0481, -0.5532,
                       -0.000136, -0.00001149);

    // Calculate U, Argument of longitude of Moon
    ls_arg.U = poly(JC_TDB, 335779.526232, 1739527262.8478, -12.7512, -0.001037,
                    0.00000417);

    // Calculate Dsun, Mean elongation of Moon from Sun
    ls_arg.DSun = poly(JC_TDB, 1072260.703692, 1602961601.2090, -6.3706,
                       0.006593, -0.00003169);

    // Calculate omega, Longitude of node of Moon
    ls_arg.omega = poly(JC_TDB, 450160.398036, -6962890.2665, 7.4722, 0.007702,
                        -0.00005939);

    ls_arg.MMoon = std::fmod(ls_arg.MMoon, 1296000.0);
    ls_arg.MSun = std::fmod(ls_arg.MSun, 1296000.0);
    ls_arg.U = std::fmod(ls_arg.U, 1296000.0);
    ls_arg.DSun = std::fmod(ls_arg.DSun, 1296000.0);
    ls_arg.omega = std::fmod(ls_arg.omega, 1296000.0);

    return ls_arg;
}

// Computes the short period tidal corrections to the smoothed IERS values for
// polar motion and UT1 - UTC.
// THESE CORRECTIONS ARE ADDED TO THE INTERPOLATED VALUES
// Original Fortran code: http://maia.usno.navy.mil/iers-gaz13
// lunar_solar_arg, units are arc-seconds.
// IERS_record, Polar motion in micro arc-seconds, UT1_minus_UTC in seconds.
// Returns GMST in micro arc-seconds.
double Frame_transform::RAY(Timestruct UTC, const lunar_solar_arg &ls_arg,
                            IERS_record &record)
{
    double arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8;
    double s_arg1, s_arg2, s_arg3, s_arg4, s_arg5, s_arg6, s_arg7, s_arg8;
    double c_arg1, c_arg2, c_arg3, c_arg4, c_arg5, c_arg6, c_arg7, c_arg8;

    double L = ls_arg.MMoon;
    // double Lprime = ls_arg.MSun; // Unused
    double capf = ls_arg.U;
    double capd = ls_arg.DSun;
    double omega = ls_arg.omega;

    double du, UT1, GMST0, GMST, theta;

    du = static_cast<double>(UTC.MJDN) - Timetag::MJD_1_jan_2000_midday;
    UT1 = static_cast<double>(UTC.SOD) + (UTC.SOD_frac + record.UT1_minus_UTC);

    GMST0 = compute_GMST0(du);
    GMST = compute_GMST(du, UT1, GMST0);

    theta = GMST - 0.5 * 1296000.0 * 1000000.0;
    theta = std::fmod(theta, 1296000.0 * 1000000.0) / 1000000.0;

    // Original calculation of theta
    // theta = std::fmod(
    //     poly(JC_TDB, 361658.22615, 47466002772.19299, 1.39656, -9.3E-5),
    //     1296000.0);

    arg1 = -2.0 * (capf + omega) + theta;
    arg2 = 2.0 * capd + arg1;
    arg3 = theta;
    arg5 = arg1 + arg3;
    arg4 = arg5 - L;
    arg6 = arg2 + arg3;

    arg1 += 972000.0;
    arg2 += 972000.0;

    arg7 = arg1 - L;
    arg8 = arg3 + arg3;

    arg3 += 324000.0;

    arg1 = std::fmod(arg1, 1296000.0) * sgnlOPS::sec_to_rad;
    arg2 = std::fmod(arg2, 1296000.0) * sgnlOPS::sec_to_rad;
    arg3 = std::fmod(arg3, 1296000.0) * sgnlOPS::sec_to_rad;
    arg4 = std::fmod(arg4, 1296000.0) * sgnlOPS::sec_to_rad;
    arg5 = std::fmod(arg5, 1296000.0) * sgnlOPS::sec_to_rad;
    arg6 = std::fmod(arg6, 1296000.0) * sgnlOPS::sec_to_rad;
    arg7 = std::fmod(arg7, 1296000.0) * sgnlOPS::sec_to_rad;
    arg8 = std::fmod(arg8, 1296000.0) * sgnlOPS::sec_to_rad;

    // clang-format off
	s_arg1 = std::sin(arg1);	c_arg1 = std::cos(arg1);
	s_arg2 = std::sin(arg2);	c_arg2 = std::cos(arg2);
	s_arg3 = std::sin(arg3);	c_arg3 = std::cos(arg3);
	s_arg4 = std::sin(arg4);	c_arg4 = std::cos(arg4);
	s_arg5 = std::sin(arg5);	c_arg5 = std::cos(arg5);
	s_arg6 = std::sin(arg6);	c_arg6 = std::cos(arg6);
	s_arg7 = std::sin(arg7);	c_arg7 = std::cos(arg7);
	s_arg8 = std::sin(arg8);	c_arg8 = std::cos(arg8);

	record.pm.x += - 133.0 * s_arg1 + 49.0 * c_arg1
                   -  50.0 * s_arg2 + 25.0 * c_arg2
                   - 152.0 * s_arg3 + 78.0 * c_arg3
                   -  57.0 * s_arg4 - 13.0 * c_arg4
                   - 330.0 * s_arg5 - 28.0 * c_arg5
                   - 145.0 * s_arg6 + 64.0 * c_arg6
                   -  26.0 * s_arg7 +  6.0 * c_arg7
                   -  36.0 * s_arg8 + 17.0 * c_arg8;

	record.pm.y += - 49.0 * s_arg1 - 133.0 * c_arg1
                   - 25.0 * s_arg2 -  50.0 * c_arg2
                   - 78.0 * s_arg3 - 152.0 * c_arg3
                   + 11.0 * s_arg4 +  33.0 * c_arg4
                   + 37.0 * s_arg5 + 196.0 * c_arg5
                   + 59.0 * s_arg6 +  87.0 * c_arg6
                   -  6.0 * s_arg7 -  26.0 * c_arg7
                   + 18.0 * s_arg8 +  22.0 * c_arg8;

	record.UT1_minus_UTC += 0.00001210 * s_arg1 + 0.00001605 * c_arg1
                          + 0.00000286 * s_arg2 + 0.00000516 * c_arg2
                          + 0.00000864 * s_arg3 + 0.00001771 * c_arg3
                          - 0.00000380 * s_arg4 - 0.00000154 * c_arg4
                          - 0.00001617 * s_arg5 - 0.00000720 * c_arg5
                          - 0.00000759 * s_arg6 - 0.00000004 * c_arg6
                          + 0.00000245 * s_arg7 + 0.00000503 * c_arg7
                          - 0.00000196 * s_arg8 - 0.00000038 * c_arg8;
    // clang-format on

    UT1 = static_cast<double>(UTC.SOD) + (UTC.SOD_frac + record.UT1_minus_UTC);

    GMST = compute_GMST(du, UT1, GMST0);
    GMST = std::fmod(GMST, 1296000.0 * 1000000.0);

    return GMST;
} // end of function RAY

// An optimised method to calculate GMST0 that maintains high precision, based
// on the method given in IERS Technical Note 21 (1996), page 21.
// Argument du is days since midday, 1st Jan 2000.
double Frame_transform::compute_GMST0(double du)
{
    constexpr double a = GMST0_a;
    constexpr double b = GMST0_b;
    constexpr double c = GMST0_c;
    constexpr double d = GMST0_d;

    // microseconds in a Julian century
    constexpr double ucent = 36525.0 * 86400.0 * 1000000.0;

    double GMST0_ab, GMST0_cd;

    double years = std::floor(du / 365.25);
    double days = du - years * 365.25;

    GMST0_cd = years * (c * 365.25 - ucent) + days * c + 36525.0 * d;

    if (GMST0_cd > ucent) {
        GMST0_cd -= ucent;
    } else if (GMST0_cd < 0.0) {
        GMST0_cd += ucent;
    }

    GMST0_ab = (du * du * (a * du + b * 36525.0)) / (10.0 * 36525.0 * 36525.0);

    // 2435 = 36525 / 15, effectively multiplying GMST0 by 15 converts the units
    // to micro arc-seconds instead of micro seconds of a day
    return (GMST0_ab + GMST0_cd) / 2435.0;
}

// An optimised method to calculate GMST that maintains high precision, based
// on the method given in IERS Technical Note 21 (1996), page 21.
// Argument du is days since midday, 1st Jan 2000.
// Argument UT1 is seconds since midnight in UT1.
// Argument GMST0 is measured in micro-arcseconds.
// GMST is returned in micro-arcseconds.
double Frame_transform::compute_GMST(double du, double UT1, double GMST0)
{
    double GMST;

    constexpr double a =
        GMST0_a / (10.0 * 36525.0 * 36525.0 * 86400.0 * 86400.0);
    constexpr double b = GMST0_b / (10.0 * 36525.0 * 86400.0);
    constexpr double c = GMST0_c;

    du *= 86400.0;

    GMST = (a * (UT1 + 3.0 * du) + b) * UT1 + (3.0 * a * du + 2.0 * b) * du + c;
    GMST /= (2435.0 * 86400.0);
    GMST += 15.0 * 1000000.0;

    GMST *= UT1;

    GMST += GMST0;

    return GMST;
}

// Calculates GMST using exactly the method from IERS Technical Note 21 (1996)
// with no optimisations. Maintained here for testing purposes.
// GMST is returned in micro-arcseconds.
// See IERS Technical Note 21 (1996), page 21
// Updated reference is IERS Technical Note 36 (2010), pages 59 - 61 and page 51
// IAU 2000 Resolution 1.8 also useful for Earth Rotation Angle
double Frame_transform::orig_compute_GMST(double du, double UT1)
{
    double r, T;

    double GMST0, GMST;

    T = du / 36525.0;

    // Calculate GMST0 in seconds, terms from page 21 of IERS technical note 21:
    GMST0 = poly(T, 24110.54841, 8640184.812866, 0.093104, -6.2E-6);

    // Compute r, the ratio of universal to sidereal time
    r = poly(T, 1.002737909350795, 5.9006E-11, -5.9E-15);

    GMST = UT1 * r + GMST0;

    // Put GMST into range (0 <= GMST < 86400)
    GMST = std::fmod(GMST, 86400.0) + ((GMST < 0.0) ? 86400.0 : 0.0);

    // Return GMST in micro-arcseconds
    return GMST * (15.0 * 1000000.0);
}

// A method to test the two methods of calculating GMST, set a start and end
// date and a desired number of comparisons. 10^8 comparisons takes ~30 seconds.
// Output for 10^9 comparisons looks like this:
//
// 1000015269 comparisons performed
//
// Biggest difference was: 3.789265950520833 ns
// GMST1: 4952.963334565918
// GMST2: 4952.963334569708
// Found at: 10/ 9/2097,  2:02:51.782769
//
// Mean difference was: -0.1296711568267801 ns
// Standard deviation was: 0.55788314976793 ns
void Frame_transform::test_GMST()
{
    Timetag start, end, test_date;
    start.set_timetag_from_cal_TT(1950, 1, 1, 0, 0, 0.0);
    end.set_timetag_from_cal_TT(2100, 1, 1, 0, 0, 0.0);

    test_date = start;

    // Only an approximate number of steps, since random is involved
    long int desired_steps = 100000000;
    long int actual_steps = 0;

    const double sec_start = start.get_TT_seconds();
    const double sec_end = end.get_TT_seconds();

    double step_size =
        2.0 * (sec_end - sec_start) / static_cast<double>(desired_steps);

    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution(0.0, step_size);

    double diff;
    double sd = 0.0;
    double mean = 0.0;
    double max = 0.0;
    double max1 = 0.0;
    double max2 = 0.0;
    Timetag max_date;

    double du, UT1, GMST0, GMST, GMST2;

    while (test_date.get_TT_seconds() < sec_end) {
        test_date.step(distribution(generator));
        actual_steps++;

        du = static_cast<double>(test_date.get_MJDN_TT()) -
             Timetag::MJD_1_jan_2000_midday;
        UT1 = static_cast<double>(test_date.get_SOD_TT()) +
              test_date.get_SOD_frac_TT();

        GMST0 = compute_GMST0(du);
        GMST = compute_GMST(du, UT1, GMST0);
        GMST = std::fmod(GMST, 1296000.0 * 1000000.0);

        GMST2 = orig_compute_GMST(du, UT1);

        diff = (GMST - GMST2) * 1000.0 / 15.0; // Find diff in ns

        mean += diff;
        sd += diff * diff;

        diff = std::abs(diff);

        if (diff > max) {
            max = diff;
            max1 = GMST / (15.0 * 1000000.0); // Convert to seconds
            max2 = GMST2 / (15.0 * 1000000.0);
            max_date = test_date;
        }
    }

    std::cout << std::endl
              << "------------------------------------------------" << std::endl
              << std::endl;

    std::cout << actual_steps << " comparisons performed" << std::endl
              << std::endl;

    std::cout << std::setprecision(16);
    std::cout << "Biggest difference was: " << max << " ns" << std::endl;
    std::cout << "GMST1: " << max1 << std::endl;
    std::cout << "GMST2: " << max2 << std::endl;
    std::cout << "Found at: ";
    max_date.print_datestamp();

    mean /= static_cast<double>(actual_steps);
    sd /= static_cast<double>(actual_steps);
    sd = std::sqrt(sd - mean * mean);

    std::cout << std::endl
              << "Mean difference was: " << mean << " ns" << std::endl;
    std::cout << "Standard deviation was: " << sd << " ns" << std::endl;
    std::cout << std::endl
              << "------------------------------------------------" << std::endl
              << std::endl;
}

// Function to calculate Greenwich Apparent Sidereal time from GMST, the
// Greenwich Mean Sidereal Time. Also uses the nutation parameters and omega,
// the mean longitude of the ascending node of the lunar orbit (radians).
//
// See IERS Technical Note 21 (1996), page 21
// Updated reference is IERS Technical Note 36 (2010), pages 59 - 61 and page 51
// IAU 2000 Resolution 1.8 also useful for Earth Rotation Angle
double Frame_transform::compute_GAST(long int MJDN, double GMST, double omega,
                                     nutation &nut)
{
    nut.E *= sgnlOPS::microsec_to_rad;
    nut.Ebar *= sgnlOPS::microsec_to_rad;

    double GAST = nut.deltaPhi * std::cos(nut.E);

    // Do this after to keep GAST in micro arc-seconds for the time being
    nut.deltaPhi *= sgnlOPS::microsec_to_rad;

    // Epoch is after 1st January 1997, therefore apply mean longitude formula
    if (MJDN >= 50449) {
        GAST += 2640.0 * std::sin(omega) + 63.0 * std::sin(2.0 * omega);
    }

    GAST += GMST;

    GAST *= sgnlOPS::microsec_to_rad;

    return GAST;
}

// Function to calculate the three main nutation arguments, obliquity of the
// ecliptic, nutation in longitude and nutation in obliquity.
nutation Frame_transform::compute_nutation(double JC_TDB,
                                           const lunar_solar_arg &ls_arg,
                                           double dEpsilon, double dPsi)
{
    nutation nut;

    // Mean anomaly for the Moon
    double MMoon[6] = {};

    // Mean anomaly for the Sun
    double MSun[5] = {};

    // Mean argument of latitude of the Moon, measured
    // on the ecliptic from the mean equinox of date
    double U[7] = {};

    // Mean elongation of the Moon from the Sun
    double DSun[7] = {};

    // Longitude of the ascending node of the mean lunar orbit
    double omega[5] = {};

    double arg;

    // Initialise deltaPhi and E
    nut.deltaPhi = 0.0;
    nut.E = 0.0;

    // Compute Ebar, obliquity of the ecliptic in micro-arcseconds
    nut.Ebar = poly(JC_TDB, 84381448000.0, -46815000.0, -590.0, 1813.0);

    MMoon[3] = ls_arg.MMoon;
    MSun[3] = ls_arg.MSun;
    U[4] = ls_arg.U;
    DSun[4] = ls_arg.DSun;
    omega[3] = ls_arg.omega;

    MMoon[5] = 3.0 * MMoon[3]; // Yes, 3.0, it's not a typo
    MMoon[4] = 2.0 * MMoon[3];
    MMoon[1] = -MMoon[3];
    MMoon[0] = -MMoon[4];

    MSun[4] = 2.0 * MSun[3];
    MSun[1] = -MSun[3];
    MSun[0] = -MSun[4];

    U[6] = 4.0 * U[4];
    U[5] = 2.0 * U[4];
    U[2] = -U[4];
    U[1] = -U[5];
    U[0] = -U[6];

    DSun[6] = 4.0 * DSun[4];
    DSun[5] = 2.0 * DSun[4];
    DSun[2] = -DSun[4];
    DSun[1] = -DSun[5];
    DSun[0] = -DSun[6];

    omega[4] = 2.0 * omega[3];
    omega[1] = -omega[3];
    omega[0] = -omega[4];

    size_t i;
    int a, b, c, d, e;

    // compute del_phi and del_E
    for (i = 105; i > 77; --i) {

        a = n_mul[i] >> 12;
        b = (n_mul[i] >> 9) & 7;
        c = (n_mul[i] >> 6) & 7;
        d = (n_mul[i] >> 3) & 7;
        e = n_mul[i] & 7;

        arg = MMoon[a] + MSun[b] + U[c] + DSun[d] + omega[e];
        nut.deltaPhi += std::sin(arg);
    }

    // Convert calculation so far to micro arc-seconds
    nut.deltaPhi *= 100.0;

    // compute del_phi and del_E
    for (i = 77; i > 64; --i) {

        a = n_mul[i] >> 12;
        b = (n_mul[i] >> 9) & 7;
        c = (n_mul[i] >> 6) & 7;
        d = (n_mul[i] >> 3) & 7;
        e = n_mul[i] & 7;

        arg = MMoon[a] + MSun[b] + U[c] + DSun[d] + omega[e];
        nut.deltaPhi += n_AC[i][0] * std::sin(arg);
    }

    // compute del_phi and del_E
    for (i = 64; i > 14; --i) {

        a = n_mul[i] >> 12;
        b = (n_mul[i] >> 9) & 7;
        c = (n_mul[i] >> 6) & 7;
        d = (n_mul[i] >> 3) & 7;
        e = n_mul[i] & 7;

        arg = MMoon[a] + MSun[b] + U[c] + DSun[d] + omega[e];
        nut.deltaPhi += n_AC[i][0] * std::sin(arg);
        nut.E += n_AC[i][1] * std::cos(arg);
    }

    // compute del_phi and del_E
    for (i = 14;; --i) {

        a = n_mul[i] >> 12;
        b = (n_mul[i] >> 9) & 7;
        c = (n_mul[i] >> 6) & 7;
        d = (n_mul[i] >> 3) & 7;
        e = n_mul[i] & 7;

        arg = MMoon[a] + MSun[b] + U[c] + DSun[d] + omega[e];
        nut.deltaPhi += (n_BD[i][0] * JC_TDB + n_AC[i][0]) * std::sin(arg);
        nut.E += (n_BD[i][1] * JC_TDB + n_AC[i][1]) * std::cos(arg);

        // Prevents error at end of loop since i is unsigned
        if (i == 0) {
            break;
        }
    }

    // Add on the interpolated IERS parameters
    nut.E += dEpsilon;
    nut.deltaPhi += dPsi;

    // Combine deltaE and Ebar
    nut.E += nut.Ebar;

    return nut;
}

bool Frame_transform::populate_IERS_coefficient_tables(std::string eop_file,
                                                       long int MJD_endday)
{
    size_t i;
    long int sentinel, current;
    double temp;
    std::string line;

    // Open EOPC4 file
    std::ifstream infile(eop_file);

    // Check that the file has opened successfully
    if (!infile.good()) {
        std::cerr << "Warning: EOPC04 file \"" << eop_file
                  << "\" could not be read in!" << std::endl;
        return false;
    }

    size_t num_days =
        static_cast<size_t>(1 + std::abs(MJD_endday - MJD_startday));

    sentinel = MJD_startday - 2;

    // Read past the eopc04 file header lines, lines 1-14 (0-13(?)) are header
    // lines. Loop is going through the header lines one-by-one, doing nothing!
    for (i = 0; i <= 13; ++i) {
        std::getline(infile, line);
    }

    IERS_parameters.resize(num_days);

    // processing the first line after the headerlines, the fourth data point
    // corresponds to the mjd of the data record.
    infile >> temp >> temp >> temp >> current;

    if (sentinel < current) {
        std::cerr << "Warning: Date requested too early for EOPC04 file."
                  << std::endl
                  << "Looking for MJD " << sentinel
                  << ", but first entry is for MJD " << current << "."
                  << std::endl;
        return false;
    }

    // search the IERSB file, extract the required values from the day before
    // and the day after the sentinel
    while (current < sentinel && std::getline(infile, line)) {
        infile >> temp >> temp >> temp >> current;
    }

    if (current > sentinel) {
        std::cerr << "Warning: Date requested is missing in EOPC04 file."
                  << std::endl
                  << "Looking for MJD " << sentinel << "." << std::endl;
        return false;
    }

    if (current != sentinel && infile.eof()) {
        std::cerr << "Warning: Date requested too recent for EOPC04 file."
                  << std::endl
                  << "Looking for MJD " << sentinel
                  << ", but last entry is for MJD " << current << "."
                  << std::endl;
        return false;
    }

    // Now we can read in and start storing data
    std::vector<IERS_record> records(num_days + 4);

    i = 0;

    // The LOD field is initially read into records[i].w, then converted to the
    // angular velocity of the Earth in the second loop
    infile >> records[i].pm.x >> records[i].pm.y >> records[i].UT1_minus_UTC >>
        records[i].w >> records[i].dPsi >> records[i].dEpsilon;

    while (i <= num_days + 2 && std::getline(infile, line)) {
        ++i;
        infile >> temp >> temp >> temp >> current;

        if (current != static_cast<long int>(i) + sentinel) {
            break;
        }

        infile >> records[i].pm.x >> records[i].pm.y >>
            records[i].UT1_minus_UTC >> records[i].w >> records[i].dPsi >>
            records[i].dEpsilon;
    }

    // Close EOPC4 file
    std::getline(infile, line);
    infile.close();

    if (current != static_cast<long int>(i) + sentinel) {
        std::cerr << "Warning: Mismatched data in EOPC04 file." << std::endl
                  << "Attempted to read in data for MJD "
                  << (static_cast<long int>(i) + sentinel)
                  << ", but found data for MJD " << current << " instead."
                  << std::endl;
        return false;
    }

    for (auto &record : records) {
        // Convert these to micro-arcseconds
        // Some head-room has been left when rounding in case IERS increase the
        // precision of these values beyond the micro-arcsecond level in future.
        record.pm.x = std::round(record.pm.x * 1.0E12) / 1.0E6;
        record.pm.y = std::round(record.pm.y * 1.0E12) / 1.0E6;
        record.dPsi = std::round(record.dPsi * 1.0E12) / 1.0E6;
        record.dEpsilon = std::round(record.dEpsilon * 1.0E12) / 1.0E6;

        // Convert LOD field to angular velocity in radian/s
        record.w = 86400.0 * 1.0E11 - std::round(record.w * 1.0E11);
        record.w *= 7.29211514670698 / (86400.0 * 1.0E16);
    }

    // At this point we have a vector of IERS_records for 2 days before and
    // after the range we need. We need the extra overlap for the interpolation.

    for (i = 0; i < num_days; i++) {
        // Don't need to interpolate w
        IERS_parameters[i].w = records[i + 2].w;

        compute_IERS_coefficients(
            records[i].pm.x, records[i + 1].pm.x, records[i + 2].pm.x,
            records[i + 3].pm.x, records[i + 4].pm.x,
            IERS_parameters[i].pm[0].x, IERS_parameters[i].pm[1].x,
            IERS_parameters[i].pm[2].x, IERS_parameters[i].pm[3].x,
            IERS_parameters[i].pm[4].x);

        compute_IERS_coefficients(
            records[i].pm.y, records[i + 1].pm.y, records[i + 2].pm.y,
            records[i + 3].pm.y, records[i + 4].pm.y,
            IERS_parameters[i].pm[0].y, IERS_parameters[i].pm[1].y,
            IERS_parameters[i].pm[2].y, IERS_parameters[i].pm[3].y,
            IERS_parameters[i].pm[4].y);

        compute_IERS_coefficients(
            records[i].UT1_minus_UTC, records[i + 1].UT1_minus_UTC,
            records[i + 2].UT1_minus_UTC, records[i + 3].UT1_minus_UTC,
            records[i + 4].UT1_minus_UTC, IERS_parameters[i].UT1_minus_UTC[0],
            IERS_parameters[i].UT1_minus_UTC[1],
            IERS_parameters[i].UT1_minus_UTC[2],
            IERS_parameters[i].UT1_minus_UTC[3],
            IERS_parameters[i].UT1_minus_UTC[4]);

        compute_IERS_coefficients(
            records[i].dPsi, records[i + 1].dPsi, records[i + 2].dPsi,
            records[i + 3].dPsi, records[i + 4].dPsi,
            IERS_parameters[i].dPsi[0], IERS_parameters[i].dPsi[1],
            IERS_parameters[i].dPsi[2], IERS_parameters[i].dPsi[3],
            IERS_parameters[i].dPsi[4]);

        compute_IERS_coefficients(
            records[i].dEpsilon, records[i + 1].dEpsilon,
            records[i + 2].dEpsilon, records[i + 3].dEpsilon,
            records[i + 4].dEpsilon, IERS_parameters[i].dEpsilon[0],
            IERS_parameters[i].dEpsilon[1], IERS_parameters[i].dEpsilon[2],
            IERS_parameters[i].dEpsilon[3], IERS_parameters[i].dEpsilon[4]);
    }

    // Everything was successful, so we return true
    return true;
} // end of function populate_IERS_coefficient_tables

// Five IERS estimates of Earth Orientation Parameters for consecutive dates
// (y1, y2, y3, y4, y5) are passed in and coefficients for a quartic that fits
// the values are calculated, a4*x^4 + a3*x^3 + a2*x^2 + a1*x + a0
void Frame_transform::compute_IERS_coefficients(double y1, double y2, double y3,
                                                double y4, double y5,
                                                double &a0, double &a1,
                                                double &a2, double &a3,
                                                double &a4)
{
    a0 = y3;
    a1 = (y1 - 8.0 * (y2 - y4) - y5) / 12.0;
    a2 = (-y1 + 16.0 * (y2 + y4) - 30.0 * y3 - y5) / 24.0;
    a3 = (-y1 + 2.0 * (y2 - y4) + y5) / 12.0;
    a4 = (y1 - 4.0 * (y2 + y4) + 6.0 * y3 + y5) / 24.0;
}

IERS_record
Frame_transform::interpolate_IERS_parameters(Timestruct UTC,
                                             long int sec_in_day) const
{
    double frac = (static_cast<double>(UTC.SOD) + UTC.SOD_frac) /
                  static_cast<double>(sec_in_day);

    size_t curr_day = static_cast<size_t>(UTC.MJDN - MJD_startday);

    IERS_param param = IERS_parameters[curr_day];

    IERS_record record;

    record.w = param.w;

    record.UT1_minus_UTC = poly(frac, param.UT1_minus_UTC[0],
                                param.UT1_minus_UTC[1], param.UT1_minus_UTC[2],
                                param.UT1_minus_UTC[3], param.UT1_minus_UTC[4]);

    record.pm.x = poly(frac, param.pm[0].x, param.pm[1].x, param.pm[2].x,
                       param.pm[3].x, param.pm[4].x);

    record.pm.y = poly(frac, param.pm[0].y, param.pm[1].y, param.pm[2].y,
                       param.pm[3].y, param.pm[4].y);

    record.dPsi = poly(frac, param.dPsi[0], param.dPsi[1], param.dPsi[2],
                       param.dPsi[3], param.dPsi[4]);

    record.dEpsilon =
        poly(frac, param.dEpsilon[0], param.dEpsilon[1], param.dEpsilon[2],
             param.dEpsilon[3], param.dEpsilon[4]);

    return record;
} // end of function interpolate_IERS_parameters
