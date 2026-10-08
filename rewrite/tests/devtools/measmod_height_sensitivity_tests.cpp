// tests/devtools/measmod_height_sensitivity_tests.cpp — how much the one-way tropospheric leg delay moves when the station height moves (SPEC-measmod.md MEAS-A-024 / MEAS-R-037; plan L0 step 8, group C8):
// the zenith delay and the mapping function against GNU bc at 30 digits, the two rows, the printed text, the verdict at both of its bounds and its failure, the command line, and the last-resort handler.
// (ctests `measmod_height_sensitivity.behaviour` and `measmod_height_sensitivity.real_tree`; the old ctest name measmod.height_sensitivity_prediction_reproduces runs the tool itself.)
//
// The Python tool had no test of its own and no recorded output.  Every expectation here is DERIVED BEFORE the code ran, from the printed formulas of TN36 section 9.1 evaluated by bc 1.07.1 (a host tool
// that names no part of the build: C8_registered/height_control.bc in this group's report files), never taken from a run of the port.

#include <catch2/catch_test_macros.hpp>

#include "throwing_stream.hpp"

#include <odl/devkit/tool.hpp>

#include "measmod_height_sensitivity.hpp"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace hs = odl::tools::measmod_height_sensitivity;
using odl::devkit::Streams;

namespace {

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = hs::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_on(const std::function<std::array<hs::Row, 2>()>& table, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = hs::run_on(table, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

bool close(double got, double want, double relative) { return std::fabs(got - want) <= relative * std::fabs(want); }

// the two printed lines, as derived by hand from the Python's f-string and the bc values below (four significant digits; no value lies within 1e-3 of a rounding boundary of its digits)
const char kLines[] =
    "dH =    30 m: dZTD = 1.627e-05 m; leg delay change at 15 deg = 1.146e-04 m (the mapping function alone 5.275e-05 m)\n"
    "dH =   100 m: dZTD = 5.423e-05 m; leg delay change at 15 deg = 3.819e-04 m (the mapping function alone 1.758e-04 m)\n";

const char kUsage[] = "usage: measmod_height_sensitivity [-h] [--check]\n";

}  // namespace

TEST_CASE("the zenith delay of TN36 9.3-9.7 and the FCULa mapping function agree with an independent evaluation by bc", "[measmod_height_sensitivity][behaviour]") {
    // bc 1.07.1, scale = 30, the formulas as printed (height_control.bc): z(h) in metres, m(s, h) dimensionless, at the IERS FCUL_A station (latitude 30.67166667 deg) and 15 degrees of elevation
    CHECK(close(hs::ztd(244.0), 1.934271318121832526540093830650, 1e-13));
    CHECK(close(hs::ztd(274.0), 1.934287588003471214181536979050, 1e-13));
    CHECK(close(hs::ztd(344.0), 1.934325552125054091051485066944, 1e-13));
    CHECK(close(hs::ztd(0.0), 1.934138999916370826145793613061, 1e-13));
    const double s15 = 0.25881904510252076234889883762404832834906890131982;   // sin(15 deg)
    CHECK(close(hs::fcula(s15, 244.0), 3.799596923785601324825815059994, 1e-13));
    CHECK(close(hs::fcula(s15, 274.0), 3.799624194867684468867952002448, 1e-13));
    CHECK(close(hs::fcula(s15, 344.0), 3.799687830594965033202648238563, 1e-13));
    CHECK(close(hs::fcula(0.5, 1000.0), 1.992662935719548567936304971459, 1e-13));
    // at the zenith the mapping function is one, whatever the height: numerator and denominator are the same continued fraction
    CHECK(hs::fcula(1.0, 0.0) == 1.0);
    CHECK(hs::fcula(1.0, 5000.0) == 1.0);
    // and it grows toward the horizon
    CHECK(hs::fcula(0.5, 244.0) > hs::fcula(0.9, 244.0));
    // the zenith delay falls as 2.8e-7 per metre in f_s (a few ppm of 1.93 m per 100 m): it grows with height here
    CHECK(hs::ztd(344.0) > hs::ztd(244.0));
}

TEST_CASE("the two rows are the changes of the delays for 30 m and 100 m above 244 m, from the independent evaluation", "[measmod_height_sensitivity][behaviour]") {
    const std::array<hs::Row, 2> rows = hs::rows();
    CHECK(rows[0].dh == 30.0);
    CHECK(rows[1].dh == 100.0);
    // bc: z(274) - z(244), z(274) m(s, 274) - z(244) m(s, 244), z(244) (m(s, 274) - m(s, 244)); and the same for 344 (a cancellation of 1e-5: a relative 1e-8 is five orders above the double's)
    CHECK(close(rows[0].d_ztd, 1.62698816386876414431483223021217352e-5, 1e-8));
    CHECK(close(rows[0].d_leg, 1.145691078095627692441354933041424586e-4, 1e-8));
    CHECK(close(rows[0].d_map, 5.27496718875717167997429216230564061e-5, 1e-8));
    CHECK(close(rows[1].d_ztd, 5.42340032215645113912365125451522272e-5, 1e-8));
    CHECK(close(rows[1].d_leg, 3.819107160196171527098406222884495581e-4, 1e-8));
    CHECK(close(rows[1].d_map, 1.758384339741903496892322158649723319e-4, 1e-8));
    // the figures of the specification (MEAS-A-024): 0.115 mm for 30 m and 0.382 mm for 100 m
    CHECK(close(rows[0].d_leg, 0.115e-3, 5e-3));
    CHECK(close(rows[1].d_leg, 0.382e-3, 5e-3));
    // the two terms add: the leg delay's change is the zenith delay's share times the mapping function plus the mapping function's share times the zenith delay
    for (const hs::Row& row : rows) CHECK(close(row.d_leg, row.d_map + row.d_ztd * hs::fcula(std::sin(15.0 * std::acos(-1.0) / 180.0), 244.0 + row.dh), 1e-6));
}

TEST_CASE("the tool prints the two rows, and with --check the verdict", "[measmod_height_sensitivity][behaviour]") {
    const Result plain = run({});
    CHECK(plain.code == 0);
    CHECK(plain.out == kLines);
    CHECK(plain.err.empty());
    const Result checked = run({"--check"});
    CHECK(checked.code == 0);
    CHECK(checked.out == std::string(kLines) + "ok       the predicted 0.115 mm and 0.382 mm reproduced\n");
    CHECK(checked.err.empty());
}

TEST_CASE("the verdict: both figures within 5e-7 of the prediction, each bound on its own, and FAILED otherwise", "[measmod_height_sensitivity][behaviour]") {
    const auto with = [](double leg30, double leg100) {
        return [=] {
            std::array<hs::Row, 2> rows = hs::rows();
            rows[0].d_leg = leg30;
            rows[1].d_leg = leg100;
            return rows;
        };
    };
    const char ok[] = "ok       the predicted 0.115 mm and 0.382 mm reproduced\n";
    const char failed[] = "FAILED   not reproduced\n";
    // inside both bounds, at 4.9e-7 either side
    CHECK(run_on(with(1.146e-4 + 4.9e-7, 3.819e-4 - 4.9e-7), {"--check"}).out.find(ok) != std::string::npos);
    CHECK(run_on(with(1.146e-4 - 4.9e-7, 3.819e-4 + 4.9e-7), {"--check"}).code == 0);
    // one figure outside its bound, 5.1e-7 away, is enough, whichever it is, whichever side
    for (const auto& damaged : {with(1.146e-4 + 5.1e-7, 3.819e-4), with(1.146e-4 - 5.1e-7, 3.819e-4), with(1.146e-4, 3.819e-4 + 5.1e-7), with(1.146e-4, 3.819e-4 - 5.1e-7), with(0.0, 0.0), with(3.819e-4, 1.146e-4)}) {
        const Result r = run_on(damaged, {"--check"});
        CHECK(r.code == 1);
        CHECK(r.out.find(failed) != std::string::npos);
        CHECK(r.out.find(ok) == std::string::npos);
        CHECK(r.err.empty());
    }
    // without --check nothing is judged
    const Result unjudged = run_on(with(0.0, 0.0), {});
    CHECK(unjudged.code == 0);
    CHECK(unjudged.out.find("ok  ") == std::string::npos);
    CHECK(unjudged.out.find("FAILED") == std::string::npos);
    // the printed rows follow the numbers handed in: dH in five columns, the three figures in four digits
    const Result printed = run_on(
        [] {
            return std::array<hs::Row, 2>{hs::Row{7.0, -1.5e-3, 2.25e-6, 9.99e-9}, hs::Row{1234.0, 0.0, 1e-10, -3.0e-4}};
        },
        {});
    CHECK(printed.out ==
          "dH =     7 m: dZTD = -1.500e-03 m; leg delay change at 15 deg = 2.250e-06 m (the mapping function alone 9.990e-09 m)\n"
          "dH =  1234 m: dZTD = 0.000e+00 m; leg delay change at 15 deg = 1.000e-10 m (the mapping function alone -3.000e-04 m)\n");
}

TEST_CASE("the command line: --check, -h; everything else is refused, exit 2", "[measmod_height_sensitivity][behaviour]") {
    const std::string help =
        std::string(kUsage) +
        "\n"
        "How much the one-way tropospheric leg delay moves when the station height H moves (SPEC-measmod.md MEAS-A-024 / MEAS-R-037): the zenith delay's own f_s(phi, H) and the mapping function's a_i3 H terms,\n"
        "for a change of 30 m and of 100 m from 244 m, at 15 degrees of elevation.\n"
        "\n"
        "options:\n"
        "  -h, --help   show this help and exit\n"
        "  --check      exit 1 unless the predicted 0.115 mm and 0.382 mm are reproduced\n"
        "\n"
        "exit codes: 0 printed (and, with --check, the two figures reproduced)   1 not reproduced   2 an argument error\n";
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run({flag});
        CHECK(r.code == 0);
        CHECK(r.out == help);
        CHECK(r.err.empty());
    }
    // -h wins over what follows it, and what precedes it is refused first
    CHECK(run({"-h", "--bogus"}).code == 0);
    CHECK(run({"--bogus", "-h"}).code == 2);
    for (const char* bad : {"--frobnicate", "stray", "--check=1", "--Check", "-c", "--", ""}) {
        const Result r = run({bad});
        CHECK(r.code == 2);
        CHECK(r.err == std::string(kUsage) + "measmod_height_sensitivity: error: unrecognized arguments: " + bad + "\n");
        CHECK(r.out.empty());
    }
    // --check twice is --check
    CHECK(run({"--check", "--check"}).code == 0);
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[measmod_height_sensitivity][behaviour]") {
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(hs::run({}, Streams{out, err}) == 70);
    CHECK(err.str() == "measmod_height_sensitivity: internal error: boom\n");
}

TEST_CASE("on the real tree --check reproduces the specification's 0.115 mm and 0.382 mm", "[measmod_height_sensitivity][real_tree]") {
    const Result r = run({"--check"});
    CHECK(r.code == 0);
    CHECK(r.out == std::string(kLines) + "ok       the predicted 0.115 mm and 0.382 mm reproduced\n");
}
