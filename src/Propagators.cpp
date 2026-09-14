/*! @file Propagators.cpp
	@author David Harrison
	@date 14 March 2016
	@brief SGNL OPS file defining the object Propagators, a father class for all
		   propagators following the factory pattern. The get_prop method can
		   return a variety of objects, all of type Propagators, depending on
		   input parameters.
 */

#include "../include/Propagators.h"

std::unique_ptr<Propagators> Propagators::get_prop(const Configuration &config,
                                                   std::string outfile)
{
    std::unique_ptr<Propagators> prop;
    int prop_num = config.propagator;

    if (prop_num == 0) {
        prop = std::make_unique<Prop_kepler>();
    } else if (prop_num == 2) {
        prop = std::make_unique<Prop_sgp4>();
    } else if (prop_num == 3) {
        prop = std::make_unique<Prop_erkf78>();
    } else if (prop_num == 4) {
        prop = std::make_unique<Prop_rk4>();
    } else {
        prop = std::make_unique<Prop_erk87>();
    }

    prop->setup(config, outfile);

    return prop;
}

void Propagators::propagate_and_print_time()
{
    auto now = std::chrono::system_clock::now();

    time_t now_t = std::chrono::system_clock::to_time_t(now);
    tm *localtm = std::localtime(&now_t); // Convert to tm struct

    constexpr size_t buffer_size = 25;
    char buffer[buffer_size];
    strftime(buffer, buffer_size, "%T on %e %b %Y", localtm);

    // Because Intel compiler doesn't seem to support std::put_time()
    // As of verison: icpc (ICC) 17.0.1 20161005

    std::cout << "Beginning simulation at: " << buffer << std::endl;
    // std::cout << "Beginning simulation at: "
    //           << std::put_time(localtm, "%T on %e %b %Y") << std::endl;

    double sim_time = propagate();

    now = std::chrono::system_clock::now();

    now_t = std::chrono::system_clock::to_time_t(now);
    localtm = std::localtime(&now_t); // Convert to tm struct

    strftime(buffer, buffer_size, "%T on %e %b %Y", localtm);

    // Because Intel compiler doesn't seem to support std::put_time()
    // As of verison: icpc (ICC) 17.0.1 20161005

    std::cout << " Finished simulation at: " << buffer << std::endl
              << std::endl;
    // std::cout << " Finished simulation at: "
    //           << std::put_time(localtm, "%T on %e %b %Y") << std::endl
    //           << std::endl;

    std::stringstream time_output;
    time_output << "        Simulation took: " << std::fixed
                << std::setprecision(6) << sim_time << " seconds.";
    std::cout << time_output.str() << std::endl;
}

double Propagators::propagate()
{
    // Output predicted orbits to file in format specified in config
    output.buffer_header(rso);
    output.buffer_output(rso);

    long int output_count = output_steps;

    auto t1 = std::chrono::steady_clock::now();

    // Inside the prediction loop
    for (long int i = total_steps; i > 0; --i) {
        step();

        // These lines are useful for testing penumbra flux scaling:
        // double t_passed = rso.get_epoch() - rso.initial_state.epoch;
        // std::cout << t_passed << ", " << rso.get_eclipse_state()
        //           << std::endl;

        if (rso.is_state_bad()) {
            output.buffer_output(rso);
            std::cerr << std::endl << rso.why_bad_state() << std::endl;
            break;
        }

        if (--output_count == 0) {
            output_count = output_steps;

            // Output predicted orbits to file
            output.buffer_output(rso);
        }
    }

    auto t2 = std::chrono::steady_clock::now();

    output.write_output(); // Make sure all output has been written.

    return std::chrono::duration<double>{t2 - t1}.count();
}

void Propagators::setup(const Configuration &config, std::string outfile)
{
    rso.setup(config);
    output.setup(outfile, config.output_format);

    set_step_size(static_cast<long double>(config.step_size));

    total_time = config.simulation_time;
    output_time = config.output_interval;

    total_steps = static_cast<long int>(std::ceil(total_time / h));

    output_steps = static_cast<long int>(std::floor(output_time / h));
    if (output_steps < 1) {
        output_steps = 1;
    }
}

void Propagators::set_step_size(long double step_size)
{
    h_LD = step_size;
    h = static_cast<double>(step_size);

    long double temp_ld;
    h_f = static_cast<double>(std::modf(h_LD, &temp_ld));
    h_i = static_cast<long int>(temp_ld);
}
