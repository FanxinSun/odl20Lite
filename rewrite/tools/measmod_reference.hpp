#pragma once
// tools/measmod_reference.hpp — reference values for SPEC-measmod.md's acceptance tests, from the DEFINITION, in 60-digit decimal arithmetic (plan L0 step 8, group C8): the generator of
// modules/measmod/tests/measmod_reference.hpp, and the library that tools/measmod_fd_sizing.cpp uses, as the Python tool imported measmod_reference.
//
// `run` is the whole tool: `measmod_reference [--check] [--header PATH] [--root DIR]`.  Exit 0 written / the committed header is what the generator writes, 1 --check found a difference (or a guard of
// the generator refused), 2 an argument error or an input file that is missing, cannot be read or is not the shape expected, 70 an error the tool did not anticipate.

#include <odl/devkit/decimal.hpp>
#include <odl/devkit/tool.hpp>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odl::tools::measmod_reference {

using Decimal = odl::devkit::Decimal;
using Values = std::vector<std::pair<std::string, double>>;     // a section: the names and the values, in the order the header lists them
using Sections = std::vector<std::pair<std::string, Values>>;   // the sections, in the order of the header
using Vec = std::array<Decimal, 3>;

/// An input file that is missing, cannot be read, or is not what the generator reads (a line of the wrong shape, a number that is not one, a record that is not there): exit 2, naming it.
struct MissingInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// One of the generator's own guards refused: the closed form and the central difference disagree, the two aberrations are not inverse, the light-time condition fails.  The Python asserted; this is exit 1.
struct GuardFailed : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// where the inputs are, relative to the tree's root (the Python's ROOT / "data/cache/...")
inline constexpr char kSlrfRelative[] = "data/cache/ilrs-slrf2020-20260205/SLRF2020_POS+VEL_2026.02.05.snx";
inline constexpr char kEccRelative[] = "data/cache/ilrs-slrecc-une-20260527/slrecc.260527.ILRS.une.snx";
inline constexpr char kHorizonsRelative[] = "data/vendored/horizons-acs3-vectors/acs3_horizons_20260925.txt";
inline constexpr char kSp3Relative[] = "data/cache/ilrs-lageos1-sp3-260103/extracted/ilrsa.orb.lageos1.260103.v80.sp3";
inline constexpr char kHeaderRelative[] = "modules/measmod/tests/measmod_reference.hpp";

/// The tree this tool was built from (ODL_TREE_ROOT).
[[nodiscard]] std::filesystem::path default_root();

/// The precision of every number here: the Python set `getcontext().prec = 60` at import.  The functions below compute in the CURRENT context and are meant to be called in this one.
inline constexpr std::int64_t kPrecision = 60;

/// PI = Decimal("3.14159265358979323846264338327950288419716939937510582097494459"), the 63 digits the Python wrote (exact: a constructor does not round).
[[nodiscard]] const Decimal& pi();
/// LN10 = Decimal(10).ln() at 60 digits, as the Python module computed it when it was imported (whatever the current context is).
[[nodiscard]] const Decimal& ln10();

/// cos by its Taylor series in 60 digits (|x| < 4 here).
[[nodiscard]] Decimal d_cos(const Decimal& x);
/// 10**x as exp(x ln 10).
[[nodiscard]] Decimal d_pow10(const Decimal& x);

/// The geodetic latitude, longitude and ellipsoidal height of a point by the fixed-point iteration tan(phi) = (z + e2 N sin(phi)) / p on the WGS 84 ellipsoid; the trigonometry at the ends is the platform's.
struct Geodetic {
    double latitude;
    double longitude;
    double height;
};
[[nodiscard]] Geodetic geodetic(const Decimal& x, const Decimal& y, const Decimal& z);

/// The two legs' times of flight in 60 digits by the closed forms (event 2: the tag is the transmit time; event 1: the bounce time).  The four vectors are positions and velocities of the station and the target.
[[nodiscard]] std::pair<Decimal, Decimal> lt_closed_form(int event, const Vec& s0, const Vec& vs, const Vec& r0, const Vec& vr);
/// The same for vectors of doubles (each element is the double exactly, as Python's Decimal(float) makes it).
[[nodiscard]] std::pair<Decimal, Decimal> lt_closed_form(int event, const std::array<double, 3>& s0, const std::array<double, 3>& vs, const std::array<double, 3>& r0, const std::array<double, 3>& vr);

// the sections of the header, each the Python's function of the same name; `root` is the tree whose data/ holds the inputs
[[nodiscard]] Values registry_section(const std::filesystem::path& root);
[[nodiscard]] Values shapiro_section();
[[nodiscard]] Values vapour_section();
[[nodiscard]] Values zenith_section();
[[nodiscard]] Values np_section();
[[nodiscard]] Values lighttime_section();
[[nodiscard]] Values position_section(const std::filesystem::path& root);
[[nodiscard]] Values aberration_section();
[[nodiscard]] Values emission_section();

/// All nine, in the order of the header.
[[nodiscard]] Sections all_sections(const std::filesystem::path& root);

/// The text of the header for these sections.
[[nodiscard]] std::string render(const Sections& sections);

/// Days from 1970-01-01 to a date of the proleptic Gregorian calendar (datetime.date's toordinal, shifted): the difference of two such numbers is `(date - date).days`.
[[nodiscard]] std::int64_t days_from_civil(int year, int month, int day);

struct Settings {
    std::filesystem::path root;
};
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::measmod_reference
