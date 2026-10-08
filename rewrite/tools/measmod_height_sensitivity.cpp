// measmod_height_sensitivity.cpp — how much the one-way tropospheric leg delay moves when the station height H moves.
//
// Plan L0 step 8, group C8, ported to C++ (the user's directive of 2026-10-06) from tools/measmod_height_sensitivity.py (deleted by the same commit: `git show bc7cd6d:rewrite/tools/measmod_height_sensitivity.py`).
//
// SPEC-measmod.md MEAS-A-024 / MEAS-R-037.  TN36 §9.1.1's footnote says the zenith-delay formula is "insensitive" to the difference between geodetic and orthometric height; the model uses the ellipsoidal height,
// so the sensitivity is stated as a number.  H enters twice: the zenith delay's own f_s(phi, H), and the mapping function's a_i3 H terms.
//
// WRITTEN 2026-10-06 BEFORE the C++ test of MEAS-A-024 was run.  The specification's first text predicted 21 um for a 100 m change from the mapping function's a_13 term alone; this evaluation (both terms, the
// printed equations of TN36 §9.1) gives 0.115 mm for 30 m and 0.382 mm for 100 m, and the specification was corrected, before the test, to say so.
//
//   measmod_height_sensitivity [--check]
//   exit 0 printed (and, with --check, the two figures reproduced)   1 not reproduced   2 an argument error   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's output was never recorded: two lines of six numbers of four digits.  The port printed them against a prediction registered BEFORE it existed and made by a route that shares
// nothing with the tool or the Python: GNU bc 1.07.1 evaluating the printed formulas of TN36 at 50 digits (C8_registered/height_control.bc, in this group's report files), whose leg delays are the 0.115 mm
// and 0.382 mm of the specification and of the tool's own --check.  (C8_proof_registration.txt, C-5, holds the registration and the result.)
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * Every floating-point operation is the Python's, in the Python's order, and none is fused (-ffp-contract=off); float ** int is the C library's pow() (devkit pymath: the compiler never folds it to a
//     multiplication), math.radians(x) is x * (pi / 180), and the sines and cosines are the C library's, called one by one.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact.  `-h` prints this tool's own text.

#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pymath.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_height_sensitivity.hpp"

#include <cmath>
#include <iostream>

namespace dk = odl::devkit;

namespace odl::tools::measmod_height_sensitivity {

using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "measmod_height_sensitivity";

// the IERS FCUL_A / FCUL_ZD_HPA test cases' station: latitude 30.67166667 deg
const double kLat = dk::py_radians(30.67166667);
constexpr double kPs = 798.4188;
constexpr double kEs = 14.322;
constexpr double kLam = 0.532;
constexpr double kTs = 15.0;

}  // namespace

double ztd(double h) {
    const double fs = 1 - 0.00266 * dk::py_cos(2 * kLat) - 0.00000028 * h;
    const double k0 = 238.0185;
    const double k2 = 57.362;
    const double k1s = 19990.975;
    const double k3s = 579.55174;
    const double sig = 1 / kLam;
    const double cco2 = 1 + 0.534e-6 * (375 - 450);
    const double fh = 1e-2 * (k1s * (k0 + dk::py_pow(sig, 2)) / dk::py_pow(k0 - dk::py_pow(sig, 2), 2) + k3s * (k2 + dk::py_pow(sig, 2)) / dk::py_pow(k2 - dk::py_pow(sig, 2), 2)) * cco2;
    const double fnh = 0.003101 * (295.235 + 3 * 2.6422 * dk::py_pow(sig, 2) + 5 * (-0.032380) * dk::py_pow(sig, 4) + 7 * 0.004028 * dk::py_pow(sig, 6));
    return 0.002416579 * fh / fs * kPs + 1e-4 * (5.316 * fnh - 3.759 * fh) * kEs / fs;
}

double fcula(double s, double h) {
    // the coefficients a_i1 .. a_i4 of TN36 Table 9.1 for i = 1, 2, 3: a_i = a0 + a1 T + a2 cos(phi) + a3 H
    struct Coefficients {
        double a0, a1, a2, a3;
    };
    constexpr Coefficients a[3] = {{12100.8e-7, 1729.5e-9, 319.1e-7, -1847.8e-11}, {30496.5e-7, 234.6e-8, -103.5e-6, -185.6e-10}, {6877.7e-5, 197.2e-7, -345.8e-5, 106.0e-9}};
    double c[3];
    for (int i = 0; i < 3; ++i) c[i] = a[i].a0 + a[i].a1 * kTs + a[i].a2 * dk::py_cos(kLat) + a[i].a3 * h;
    return (1 + c[0] / (1 + c[1] / (1 + c[2]))) / (s + c[0] / (s + c[1] / (s + c[2])));
}

std::array<Row, 2> rows() {
    const double s = dk::py_sin(dk::py_radians(15.0));
    const double h0 = 244.0;
    std::array<Row, 2> out{};
    std::size_t i = 0;
    for (const double dh : {30.0, 100.0}) {
        const double d_leg = ztd(h0 + dh) * fcula(s, h0 + dh) - ztd(h0) * fcula(s, h0);
        const double d_map = ztd(h0) * (fcula(s, h0 + dh) - fcula(s, h0));
        out[i++] = Row{dh, ztd(h0 + dh) - ztd(h0), d_leg, d_map};
    }
    return out;
}

namespace {

const char kUsageText[] = "usage: measmod_height_sensitivity [-h] [--check]\n";

const char kHelpText[] =
    "\n"
    "How much the one-way tropospheric leg delay moves when the station height H moves (SPEC-measmod.md MEAS-A-024 / MEAS-R-037): the zenith delay's own f_s(phi, H) and the mapping function's a_i3 H terms,\n"
    "for a change of 30 m and of 100 m from 244 m, at 15 degrees of elevation.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --check      exit 1 unless the predicted 0.115 mm and 0.382 mm are reproduced\n"
    "\n"
    "exit codes: 0 printed (and, with --check, the two figures reproduced)   1 not reproduced   2 an argument error\n";

}  // namespace

int run_on(const std::function<std::array<Row, 2>()>& table_of, const std::vector<std::string>& argv, Streams io) {
    try {
        bool check = false;
        for (const std::string& arg : argv) {
            if (arg == "-h" || arg == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (arg == "--check") {
                check = true;
            } else {
                io.err << kUsageText << kTool << ": error: unrecognized arguments: " << arg << '\n';
                return kArgument;
            }
        }
        const std::array<Row, 2> table = table_of();
        for (const Row& row : table) {
            io.out << "dH = " << dk::pad_left(dk::py_format_f(row.dh, 0), 5) << " m: dZTD = " << dk::py_format_e(row.d_ztd, 3) << " m; leg delay change at 15 deg = " << dk::py_format_e(row.d_leg, 3)
                   << " m (the mapping function alone " << dk::py_format_e(row.d_map, 3) << " m)\n";
        }
        if (check) {
            const bool ok = std::fabs(table[0].d_leg - 1.146e-4) < 5e-7 && std::fabs(table[1].d_leg - 3.819e-4) < 5e-7;
            io.out << (ok ? "ok       the predicted 0.115 mm and 0.382 mm reproduced" : "FAILED   not reproduced") << '\n';
            return ok ? kOk : kFailed;
        }
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) {
    return run_on([] { return rows(); }, argv, io);
}

}  // namespace odl::tools::measmod_height_sensitivity

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::measmod_height_sensitivity::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
