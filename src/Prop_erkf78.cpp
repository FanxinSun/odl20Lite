/*! @file Prop_erkf78.cpp
	@author Zhen Li
	@date 15 November 2016
	@brief SGNL OPS file defining an Embedded Runge-Kutta-Fehlberg 7(8) Integrator
 */

#include "../include/Prop_erkf78.h"

const int Prop_erkf78::m_bRow = 13;
const int Prop_erkf78::m_bCol = 12;

// Source for coefficients:
// https://github.com/nasa/trick/blob/master/trick_source/er7_utils/integration/
// rkf78/src/rkf78_butcher_tableau.cc

// clang-format off
const double Prop_erkf78::m_c[13] = {          0.0,   2.0 / 27.0,    1.0 / 9.0,
              1.0 / 6.0,  5.0 / 12.0,    1.0 / 2.0,    5.0 / 6.0,    1.0 / 6.0,
              2.0 / 3.0,   1.0 / 3.0,          1.0,          0.0,          1.0};

const double Prop_erkf78::m_b[13] = {          0.0,          0.0,          0.0,
                    0.0,         0.0, 34.0 / 105.0,   9.0 / 35.0,   9.0 / 35.0,
            9.0 / 280.0, 9.0 / 280.0,          0.0, 41.0 / 840.0, 41.0 / 840.0};

const double Prop_erkf78::m_bhat[13] = {41.0 / 840.0,        0.0,          0.0,
                      0.0,         0.0, 34.0 / 105.0, 9.0 / 35.0,   9.0 / 35.0,
              9.0 / 280.0, 9.0 / 280.0, 41.0 / 840.0,        0.0,          0.0};

const double Prop_erkf78::m_a[13][12] = {{
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
      2.0/27.0,       0.0,        0.0,          0.0,           0.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
      1.0/36.0,  1.0/12.0,        0.0,          0.0,           0.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
      1.0/24.0,       0.0,    1.0/8.0,          0.0,           0.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
      5.0/12.0,       0.0, -25.0/16.0,    25.0/16.0,           0.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
      1.0/20.0,       0.0,        0.0,      1.0/4.0,       1.0/5.0,         0.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
   -25.0/108.0,       0.0,        0.0,  125.0/108.0,    -65.0/27.0,  125.0/54.0,
           0.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
    31.0/300.0,       0.0,        0.0,          0.0,    61.0/225.0,    -2.0/9.0,
    13.0/900.0,       0.0,        0.0,          0.0,           0.0,         0.0
},{
           2.0,       0.0,        0.0,    -53.0/6.0,    704.0/45.0,  -107.0/9.0,
     67.0/90.0,       3.0,        0.0,          0.0,           0.0,         0.0
},{
   -91.0/108.0,       0.0,        0.0,   23.0/108.0,  -976.0/135.0,  311.0/54.0,
    -19.0/60.0,  17.0/6.0,  -1.0/12.0,          0.0,           0.0,         0.0
},{
 2383.0/4100.0,       0.0,        0.0, -341.0/164.0, 4496.0/1025.0, -301.0/82.0,
 2133.0/4100.0, 45.0/82.0, 45.0/164.0,    18.0/41.0,           0.0,         0.0
},{
     3.0/205.0,       0.0,        0.0,          0.0,           0.0,   -6.0/41.0,
    -3.0/205.0, -3.0/41.0,   3.0/41.0,     6.0/41.0,           0.0,         0.0
},{
-1777.0/4100.0,       0.0,        0.0, -341.0/164.0, 4496.0/1025.0, -289.0/82.0,
 2193.0/4100.0, 51.0/82.0, 33.0/164.0,    12.0/41.0,           0.0,         1.0
}};
// clang-format on

// here y0 should be dphi/dt and velocity and acceleration
void Prop_erkf78::step()
{
    State_vector Xo = rso.get_eci(); // Initial state vector
    Matrix6x6 PhiMo = rso.phiM;      // Initial state transition matrix

    State_vector Xi; // i-th state vector
    Matrix6x6 PhiMi; // i-th Phi matrix

    State_vector dXidt[m_bRow]; // Time derivatives of ith State_vector.
    Matrix6x6 dPhidt[m_bRow];   // Time derivatives of ith Phi matrix.

    Cartesian a = rso.get_acceleration();
    Matrix6x6 dfdy = rso.get_partial_derivatives();

    // Calculate time derivatives
    dXidt[0].set(Xo.u, Xo.v, Xo.w, a.x, a.y, a.z, Xo.epoch);
    dPhidt[0] = dfdy * PhiMo;

    for (int i = 1; i < m_bRow; ++i) {

        Xi = m_a[i][0] * dXidt[0];
        Xi.epoch.step(m_c[i] * h);

        PhiMi = m_a[i][0] * dPhidt[0];

        for (int j = 1; j < i; ++j) {
            Xi += m_a[i][j] * dXidt[j];
            PhiMi += m_a[i][j] * dPhidt[j];
        }

        Xi = Xi * h + Xo;
        PhiMi = PhiMi * h + PhiMo;

        rso.update_with_acc_and_deriv(Xi);

        a = rso.get_acceleration();
        dfdy = rso.get_partial_derivatives();

        // Calculate time derivatives
        dXidt[i].set(Xi.u, Xi.v, Xi.w, a.x, a.y, a.z);
        dPhidt[i] = dfdy * PhiMi;

        /* Note that, dPHI/dt = dF/dy * PHI
         PHI = dy/dy0 ; PHI(t0) = I

         df/dy is like this :
            |0        I    |
            |              |
            |da/dr   da/dv |

         if considering the sensitivity matrix S:
          S = dy/dp ; S(t0) = 0

                                |  0    |
         dS/dt = df/dy * S +    |       |
                                | da/dp |, 6 by np

         dPHI/dt = df/dy*PHI

                                         |0   0     |
         d(PHI,S)/dt = dF/dy * (PHI,S) + |          |
                                         |0   da/dp |

         */
    }

    // Calculate final state:

    State_vector Xf; // final state vector
    Matrix6x6 PhiMf; // final Phi matrix

    Xf = m_b[5] * dXidt[5];
    Xf.epoch = Xo.epoch;
    Xf.epoch.step(h_i, h_f);

    PhiMf = m_b[5] * dPhidt[5];

    for (int i = 6; i < m_bRow; ++i) {
        Xf += m_b[i] * dXidt[i];
        PhiMf += m_b[i] * dPhidt[i];
    }

    Xf = Xf * h + Xo;
    PhiMf = PhiMf * h + PhiMo;

    rso.phiM = PhiMf;
    rso.update_with_acc_and_deriv(Xf);

    // Calculate error state:

    // State_vector Xe; // error estimate of state vector
    // Matrix6x6 PhiMe; // error estimate of Phi matrix
    //
    // Xe = (m_b[0] - m_bhat[0]) * dXidt[0];
    // Xe.epoch = Xf.epoch;
    //
    // PhiMe = (m_b[0] - m_bhat[0]) * dPhidt[0];
    //
    // for (int i = 10; i < m_bRow; ++i) {
    //     Xe += (m_b[i] - m_bhat[i]) * dXidt[i];
    //     PhiMe += (m_b[i] - m_bhat[i]) * dPhidt[i];
    // }
    //
    // Xe *= h;
    // PhiMe *= h;
}
