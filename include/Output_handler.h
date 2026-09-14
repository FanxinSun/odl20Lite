/*! @file Output_handler.h
	@author David Harrison
	@date 16 July 2017
	@brief Output_handler is responsible for writing simulation data to a file.
 */

#ifndef SGNL_OUTPUT_HANDLER_H
#define SGNL_OUTPUT_HANDLER_H

#include "Resident_space_object.h"

enum OutputFormat { OPS, ALL, ECI, ECEF, NONE, STORE, ATMOS };

class Output_handler
{
  private:
    std::string output_file;
    OutputFormat output_format = OPS; // Default to UCL OPS format

    std::vector<std::tuple<State_vector, Matrix6x6, Matrix6x5>> store;

    // Collect 1 MiB of output into buffer before writing
    static constexpr int buffer_limit = 1024 * 1024;
    std::stringstream output_buffer;

  public:
    Output_handler() = default;

    Output_handler(std::string file) : output_file{file}
    {
        truncate_output_file();
    }

    Output_handler(std::string file, std::string format) : output_file{file}
    {
        set_output_format(format);
        truncate_output_file();
    }

    void setup(std::string file)
    {
        output_file = file;
        truncate_output_file();
    }

    void setup(std::string file, std::string format)
    {
        output_file = file;
        set_output_format(format);
        truncate_output_file();
    }

    void set_output_format(std::string format)
    {
        if (format == "eci" || format == "ECI") {
            output_format = ECI;
        } else if (format == "ecef" || format == "ECEF") {
            output_format = ECEF;
        } else if (format == "all" || format == "ALL") {
            output_format = ALL;
        } else if (format == "none" || format == "NONE") {
            output_format = NONE;
        } else if (format == "store" || format == "STORE") {
            output_format = STORE;
        } else if (format == "atmos" || format == "ATMOS") {
            output_format = ATMOS;
        } else {
            output_format = OPS;
        }
    }

    // Deletes contents of output file
    void truncate_output_file()
    {
        if (output_file != "" && output_format != NONE &&
            output_format != STORE) {
            std::ofstream out_file(output_file,
                                   std::ofstream::out | std::ofstream::trunc);
        }
    }

    void clear_store()
    {
        store.clear();
    }

    void reserve_store(size_t max)
    {
        store.reserve(max);
    }

    std::vector<std::tuple<State_vector, Matrix6x6, Matrix6x5>> get_store() const
    {
        return store;
    }

    std::string header_full_state();
    void stream_full_state(const Resident_space_object &rso);

    void stream_output_JPL(State_vector out);

    void buffer_header(const Resident_space_object &rso);
    void buffer_output(const Resident_space_object &rso);
    void write_output();
};

#endif
