/*! @file Prop_erk87.cpp
	@author Santosh Bhattarai
	@date 17 February 2015
	@brief SGNL OPS file defining an Embedded Runge-Kutta 8(7) Integrator
 */

#include "../include/Prop_erk87.h"

/*
 * Since we do not currently use the error estimate, I have commented out all
 * code relating to it, including one line in the header file.
 * David Harrison, 5th December 2015
 */
void Prop_erk87::step()
{
    int i, j;

    const State_vector Xo = rso.get_eci(); // Save initial state
    State_vector dXidt[12];                // Time derivatives of ith State Vec.

    Cartesian a = rso.get_acceleration();

    // We set the epoch to Xo.epoch here so that all future Xi state vectors
    // will also be initialised to that value. But we don't bother setting the
    // epoch of the other dXidt State Vectors as we never need it.
    dXidt[0].set(Xo.u, Xo.v, Xo.w, a.x, a.y, a.z, Xo.epoch);

    // Arithmetic operations on State Vectors don't affect the epoch, therefore
    // Xf.epoch is also initialised to Xo.epoch here.
    State_vector Xf = h_b[0] * dXidt[0];
    Xf.epoch.step(h_i, h_f); // Set epoch of the final state

    // State_vector Xe = h_bhat[0] * dXidt[0];

    /*
	 * h_b and h_bhat are both 0 for i = 1, 2, 3 and 4, so Xf and Xe are
	 * not updated in this loop.
	 */
    for (i = 1; i < 5; ++i) {
        State_vector Xi = h_a[i][0] * dXidt[0]; // i-th state vector
        Xi.epoch.step(h_ci[i], h_cf[i]);

        for (j = 1; j < i; ++j) {
            Xi += h_a[i][j] * dXidt[j];
        }

        Xi += Xo;

        rso.update_with_acc(Xi);
        a = rso.get_acceleration();

        dXidt[i].set(Xi.u, Xi.v, Xi.w, a.x, a.y, a.z);
    }

    /*
	 * Xf and Xe ARE updated in this loop, but now the inner loop is slightly
	 * different.
	 */
    for (i = 5; i < 12; ++i) {
        State_vector Xi = h_a[i][0] * dXidt[0];
        Xi.epoch.step(h_ci[i], h_cf[i]);

        // RK8_a[i][1] and RK8_a[i][2] are always 0 for i>4, so we can skip them
        for (j = 3; j < i; ++j) {
            Xi += h_a[i][j] * dXidt[j];
        }

        Xi += Xo;

        rso.update_with_acc(Xi);
        a = rso.get_acceleration();

        dXidt[i].set(Xi.u, Xi.v, Xi.w, a.x, a.y, a.z);

        Xf += h_b[i] * dXidt[i];
        // Xe += h_bhat[i] * dXidt[i];
    }

    /*
    {
        i = 12;

        State_vector Xi = h_a[i][0] * dXidt[0];
        Xi.epoch.step(h_ci[i - 1], h_cf[i - 1]);

        // RK8_a[i][1] and RK8_a[i][2] are always 0 for i>4, so we can skip them
        for (j = 3; j < i - 1; ++j) {
            Xi += h_a[12][j] * dXidt[j];
        }

        Xi += Xo;

        rso.update_with_acc(Xi);
        a = rso.get_acceleration();

        // Re-use previous dXidt
        dXidt[i-1].set(Xi.u, Xi.v, Xi.w, a.x, a.y, a.z);

        Xe += h_bhat[i] * dXidt[i-1];
    }

    // Calculate error:
    Xe = Xf - Xe;
    */

    // Calculate final State Vector
    Xf += Xo;

    rso.update_with_acc(Xf);
}

// Populate erk87 coefficient arrays
void Prop_erk87::set_step_size(long double step_size)
{
    Propagators::set_step_size(step_size);

    long double temp_ld;
    long double h_c[12] = {};

    h_c[1] = h_LD * 1.0L / 18.0L;
    h_c[2] = h_LD * 1.0L / 12.0L;
    h_c[3] = h_LD * 1.0L / 8.0L;
    h_c[4] = h_LD * 5.0L / 16.0L;
    h_c[5] = h_LD * 3.0L / 8.0L;
    h_c[6] = h_LD * 59.0L / 400.0L;
    h_c[7] = h_LD * 93.0L / 200.0L;
    h_c[8] = h_LD * 5490023248.0L / 9719169821.0L;
    h_c[9] = h_LD * 13.0L / 20.0L;
    h_c[10] = h_LD * 1201146811.0L / 1299019798.0L;
    h_c[11] = h_LD * 1.0L;

    for (int i = 1; i < 12; ++i) {
        h_cf[i] = static_cast<double>(std::modf(h_c[i], &temp_ld));
        h_ci[i] = static_cast<long int>(temp_ld);
    }

    h_a[1][0] = h_LD * 1.0L / 18.0L;

    h_a[2][0] = h_LD * 1.0L / 48.0L;
    h_a[2][1] = h_LD * 1.0L / 16.0L;

    h_a[3][0] = h_LD * 1.0L / 32.0L;
    h_a[3][2] = h_LD * 3.0L / 32.0L;

    h_a[4][0] = h_LD * 5.0L / 16.0L;
    h_a[4][2] = h_LD * -75.0L / 64.0L;
    h_a[4][3] = h_LD * 75.0L / 64.0L;

    h_a[5][0] = h_LD * 3.0L / 80.0L;
    h_a[5][3] = h_LD * 3.0L / 16.0L;
    h_a[5][4] = h_LD * 3.0L / 20.0L;

    h_a[6][0] = h_LD * 29443841.0L / 614563906.0L;
    h_a[6][3] = h_LD * 77736538.0L / 692538347.0L;
    h_a[6][4] = h_LD * -28693883.0L / 1125000000.0L;
    h_a[6][5] = h_LD * 23124283.0L / 1800000000.0L;

    h_a[7][0] = h_LD * 16016141.0L / 946692911.0L;
    h_a[7][3] = h_LD * 61564180.0L / 158732637.0L;
    h_a[7][4] = h_LD * 22789713.0L / 633445777.0L;
    h_a[7][5] = h_LD * 545815736.0L / 2771057229.0L;
    h_a[7][6] = h_LD * -180193667.0L / 1043307555.0L;

    h_a[8][0] = h_LD * 39632708.0L / 573591083.0L;
    h_a[8][3] = h_LD * -433636366.0L / 683701615.0L;
    h_a[8][4] = h_LD * -421739975.0L / 2616292301.0L;
    h_a[8][5] = h_LD * 100302831.0L / 723423059.0L;
    h_a[8][6] = h_LD * 790204164.0L / 839813087.0L;
    h_a[8][7] = h_LD * 800635310.0L / 3783071287.0L;

    h_a[9][0] = h_LD * 246121993.0L / 1340847787.0L;
    h_a[9][3] = h_LD * -37695042795.0L / 15268766246.0L;
    h_a[9][4] = h_LD * -309121744.0L / 1061227803.0L;
    h_a[9][5] = h_LD * -12992083.0L / 490766935.0L;
    h_a[9][6] = h_LD * 6005943493.0L / 2108947869.0L;
    h_a[9][7] = h_LD * 393006217.0L / 1396673457.0L;
    h_a[9][8] = h_LD * 123872331.0L / 1001029789.0L;

    h_a[10][0] = h_LD * -1028468189.0L / 846180014.0L;
    h_a[10][3] = h_LD * 8478235783.0L / 508512852.0L;
    h_a[10][4] = h_LD * 1311729495.0L / 1432422823.0L;
    h_a[10][5] = h_LD * -10304129995.0L / 1701304382.0L;
    h_a[10][6] = h_LD * -48777925059.0L / 3047939560.0L;
    h_a[10][7] = h_LD * 15336726248.0L / 1032824649.0L;
    h_a[10][8] = h_LD * -45442868181.0L / 3398467696.0L;
    h_a[10][9] = h_LD * 3065993473.0L / 597172653.0L;

    h_a[11][0] = h_LD * 185892177.0L / 718116043.0L;
    h_a[11][3] = h_LD * -3185094517.0L / 667107341.0L;
    h_a[11][4] = h_LD * -477755414.0L / 1098053517.0L;
    h_a[11][5] = h_LD * -703635378.0L / 230739211.0L;
    h_a[11][6] = h_LD * 5731566787.0L / 1027545527.0L;
    h_a[11][7] = h_LD * 5232866602.0L / 850066563.0L;
    h_a[11][8] = h_LD * -4093664535.0L / 808688257.0L;
    h_a[11][9] = h_LD * 3962137247.0L / 1805957418.0L;
    h_a[11][10] = h_LD * 65686358.0L / 487910083.0L;

    h_a[12][0] = h_LD * 403863854.0L / 491063109.0L;
    h_a[12][3] = h_LD * -5068492393.0L / 434740067.0L;
    h_a[12][4] = h_LD * -411421997.0L / 543043805.0L;
    h_a[12][5] = h_LD * 652783627.0L / 914296604.0L;
    h_a[12][6] = h_LD * 11173962825.0L / 925320556.0L;
    h_a[12][7] = h_LD * -13158990841.0L / 6184727034.0L;
    h_a[12][8] = h_LD * 3936647629.0L / 1978049680.0L;
    h_a[12][9] = h_LD * -160528059.0L / 685178525.0L;
    h_a[12][10] = h_LD * 248638103.0L / 1413531060.0L;

    h_b[0] = h_LD * 13451932.0L / 455176623.0L;
    h_b[5] = h_LD * -808719846.0L / 976000145.0L;
    h_b[6] = h_LD * 1757004468.0L / 5645159321.0L;
    h_b[7] = h_LD * 656045339.0L / 265891186.0L;
    h_b[8] = h_LD * -3867574721.0L / 1518517206.0L;
    h_b[9] = h_LD * 465885868.0L / 322736535.0L;
    h_b[10] = h_LD * 53011238.0L / 667516719.0L;
    h_b[11] = h_LD * 2.0L / 45.0L;

    /*
    h_bhat[0] = h_LD * 14005451.0L / 335480064.0L;
    h_bhat[5] = h_LD * -59238493.0L / 1068277825.0L;
    h_bhat[6] = h_LD * 181606767.0L / 758867731.0L;
    h_bhat[7] = h_LD * 561292985.0L / 797845732.0L;
    h_bhat[8] = h_LD * -1041891430.0L / 1371343529.0L;
    h_bhat[9] = h_LD * 760417239.0L / 1151165299.0L;
    h_bhat[10] = h_LD * 118820643.0L / 751138087.0L;
    h_bhat[11] = h_LD * -528747749.0L / 2220607170.0L;
    h_bhat[12] = h_LD * 1.0L / 4.0L;
    */
}
