#pragma once
// tools/measmod_height_sensitivity.hpp — how much the one-way tropospheric leg delay moves when the station height H moves (SPEC-measmod.md MEAS-A-024 / MEAS-R-037; plan L0 step 8, group C8).
//
// `run` is the whole tool: `measmod_height_sensitivity [--check]`.  Exit 0 printed (and, with --check, the two figures reproduced), 1 not reproduced, 2 an argument error, 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace odl::tools::measmod_height_sensitivity {

/// The zenith delay of TN36 eqs (9.3)-(9.7) at the IERS FCUL_A / FCUL_ZD_HPA test cases' station, as a function of the ellipsoidal height (metres): the Python's ztd().
[[nodiscard]] double ztd(double height);

/// The FCULa mapping function of TN36 Table 9.1 / eq. (9.9) at sin(elevation) `s` and height `height`: the Python's fcula().
[[nodiscard]] double fcula(double s, double height);

/// One of the two lines the tool prints: the change dH of the height, the zenith delay's change, the leg delay's change at 15 degrees, and the mapping function's alone.
struct Row {
    double dh;
    double d_ztd;
    double d_leg;
    double d_map;
};

/// The two rows, for dH = 30 m and 100 m from H = 244 m.
[[nodiscard]] std::array<Row, 2> rows();

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` takes the function that makes the two rows (a test hands in damaged ones, to show the verdict firing); `run` uses rows().
[[nodiscard]] int run_on(const std::function<std::array<Row, 2>()>& table, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::measmod_height_sensitivity
