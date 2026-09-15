/*! @file Prop_sgp4.cpp
	@author David Harrison
	@date 5 May 2016
	@brief SGNL OPS file defining an SPG4 propagator for Two Line Elements.
 */

#include "../include/Prop_sgp4.h"

void Prop_sgp4::step()
{
    if (rso.sgp4_state.step_TLE_time(h_i, h_f)) {

        // SGP4's state is in TEME, not J2000. Converting here keeps every
        // frame downstream consistent; without it the position carries a
        // silent rotation of tens of kilometres that preserves |r|.
        rso.update_from_teme(rso.sgp4_state.get_current_state());

    } else {

        std::cerr << "TLE is in an invalid state!" << std::endl
                  << std::endl
                  << "Here is the state vector at TLE epoch:" << std::endl;
        rso.sgp4_state.get_epoch_state().print();

        std::cerr << std::endl
                  << "Here is the last valid state vector:" << std::endl;
        rso.sgp4_state.get_current_state().print();

        std::cerr << std::endl << "Exiting now." << std::endl;
        std::exit(1);
    }
}
