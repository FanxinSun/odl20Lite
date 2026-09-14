/*! @file Attitude_state.h
	@author Santosh Bhattarai
	@date 1 July 2015
	@brief SGNL OPS header file for defining the attitude state of a resident
		   space object.
 */

#ifndef SGNL_ATTITUDE_STATE_H
#define SGNL_ATTITUDE_STATE_H

#include "Matrix.h"

/**
 * @class Attitude_state
 * @author Santosh Bhattarai
 * @date 1 July 2015
 *
 * @brief This is a class that deals with attitude state information for RSO's.
 *
 * The attitude state of an RSO describes the orientation of the body frame of
 * that RSO with respect to the orientation of another coordinate frame, which
 * will be called the reference frame. Here, unless stated otherwise the
 * reference frame will the Earth Centred Inertial frame (J2000). For every RSO
 * State_vector object, there should be an associated Attitude_state object.
 */
class Attitude_state
{
  public:
    // Attributes
    // The first and perhaps the simplest way of describing the attitude
    // state of an object would be using the unit vectors of the body frame
    // system express in the ECI basis.
    Cartesian x_hat; // unit-vector of the body-frame x-direction
    Cartesian y_hat; // unit-vector of the body-frame y-direction
    Cartesian z_hat; // unit-vector of the body-frame z-direction
    Cartesian panel; // unit-vector of the solar panel normal direction

    Matrix3x3 dxdr = Matrix3x3::Zero();
    Matrix3x3 dydr = Matrix3x3::Zero();
    Matrix3x3 dzdr = Matrix3x3::Zero();
    Matrix3x3 dpaneldr = Matrix3x3::Zero();

    Attitude_state(); // default constructor
    Attitude_state(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                   Cartesian panel);
    Attitude_state(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                   Cartesian panel, Matrix3x3 dxdr, Matrix3x3 dydr,
                   Matrix3x3 dzdr, Matrix3x3 dpaneldr);

    /**** member functions ****/
    void set(Cartesian in_x, Cartesian in_y, Cartesian in_z, Cartesian panel);
    void set(Cartesian in_x, Cartesian in_y, Cartesian in_z, Cartesian panel,
             Matrix3x3 dxdr, Matrix3x3 dydr, Matrix3x3 dzdr,
             Matrix3x3 dpaneldr);
    void set_norm(Cartesian in_x, Cartesian in_y, Cartesian in_z,
                  Cartesian panel);

    // Yaw steering reference is:
    // "GNSS satellite geometry and attitude models", O. Montenbruck, 2015

    // IGS definition for Yaw Steering implies that the unit vector in Sun
    // direction and the positive x-axis of the frame are always part of the
    // same hemisphere. ie: +x face is on sunlit side, -x face is dark

    // Yaw steering may also be anti-parallel to the IGS definition
    // ie: with -x face sunlit and +x face dark
    // GPS: "Block IIR satellites employ a yaw-steering attitude but keep the
    // negative xBF-face pointing toward the Sun."
    // Galileo: "Similar to GPS, the positive zBF- and yBF-axes are aligned with
    // the antenna boresight direction and the solar panel rotation axis,
    // respectively, but the positive xBF-panel is oriented away from the Sun
    // during nominal yaw-steering."

    // Therefore a flag is included to specify whether attitude law follows IGS
    // definition, or is opposite to IGS definition (with x and y axes flipped).
    void set_yaw_steering(Cartesian rso, Cartesian sun, bool follow_IGS);

    void print();
};

#endif
