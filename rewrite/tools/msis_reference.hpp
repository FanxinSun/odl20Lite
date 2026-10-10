#pragma once
// tools/msis_reference.hpp — freeze the NRLMSISE-00 reference implementation's output (SPEC-atmosphere ATMO-R-028; plan L0 step 8, group C10).
//
// `run` is the whole tool: `msis_reference [--root DIR] [--out HEADER] [--check] [--work DIR] [--sweep-input FILE]`.  It builds NRL's published Fortran (the manifest entry `nrlmsise00-fortran`, lines
// 1..2437, UNEDITED) twice with the HOST PROGRAM gfortran — as published, and with every REAL widened — runs both on the 17 published cases and a sweep, classifies the 2,172 comparisons and writes
// modules/atmosphere/src/msis_reference_values.hpp.  The user's decision of 2026-10-06 ("The Fortran oracle stays") is recorded in the .cpp: model.f stays Fortran as published, the 65-line driver and the
// wrapper are C++.  gfortran is a regeneration-only host program: nothing in the build, the tests or CI runs it (ATMO-R-028), and the tool's own tests use a stand-in compiler.  Exit 0 written / the committed
// header reproduces / no compiler on PATH (nothing regenerated); 1 the committed header differs, a compiler or a reference run failed, or the pinned bytes are not the cached bytes (the Python's
// sys.exit(message)); 2 an argument error or an input that is missing or cannot be read; 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odl::tools::msis_reference {

inline constexpr char kTool[] = "msis_reference";
inline constexpr char kSourceId[] = "nrlmsise00-fortran";
inline constexpr char kDefaultOut[] = "modules/atmosphere/src/msis_reference_values.hpp";
inline constexpr std::size_t kDriverSplit = 2437;   // the model is lines 1..2437; the distribution's own driver follows

/// What the Python ended with sys.exit(message): a compiler or a reference run that failed, bytes that are not the pinned bytes.  Exit 1, the message alone on standard error.
struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/// An input that is missing, unreadable or not the shape this reads (the Python's OSError, KeyError, ValueError, IndexError): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// A comparison's address: kind "g7" (GTD7: nine densities and two temperatures) or "g7d" (GTD7D: the total density), set "P" (a published case) or "S" (a sweep point), and the 1-based index in the set.
struct Key {
    std::string kind;
    std::string set;
    long long index = 0;
    friend bool operator==(const Key&, const Key&) = default;
    friend auto operator<=>(const Key&, const Key&) = default;
};
/// The reference's output, in the order its lines came (a repeated key replaces the values and keeps its place, as a Python dict does).
struct Results {
    std::vector<std::pair<Key, std::vector<double>>> items;
    [[nodiscard]] const std::vector<double>* find(const Key& key) const;
    void set(const Key& key, std::vector<double> values);
};

struct Comparison {
    double relative = 0.0;
    Key key;
    std::size_t quantity = 0;
};
struct Underflow {
    Key key;
    std::size_t quantity = 0;
    double value = 0.0;   // the double-precision value, where single returned exactly zero
};
struct Classes {
    std::vector<Comparison> material;     // A: sorted, the largest relative difference first
    std::vector<Comparison> immaterial;   // B: sorted likewise
    std::vector<Underflow> underflowed;   // C
    std::size_t zeros = 0;                // Z
};

inline constexpr double kMaterial = 1e-15;   // a species whose mass is less than this fraction of the total density cannot affect drag

/// The altitudes (km) and the conditions (day of year, seconds, latitude, longitude, F10.7 average, F10.7, Ap) of the sweep.
[[nodiscard]] const std::vector<double>& sweep_alt();
struct Condition {
    int iday;
    double sec, lat, lon, f107a, f107, ap;
};
[[nodiscard]] const std::vector<Condition>& sweep_cond();

/// The Fortran driver: the 17 published cases verbatim, then a sweep read from standard input.
[[nodiscard]] std::string driver_source();
/// Lines 1..kDriverSplit of the cached source (universal newlines; split at "\n" only), each ended by "\n".
[[nodiscard]] std::string model_source(std::string_view fortran_text);
/// The sweep as the driver reads it: the number of cases, then one line per case.
[[nodiscard]] std::string sweep_input();
/// The reference's standard output: the lines "G7 tag index 11 numbers" and "G7D tag index 1 number".  Throws BadInput for a line that does not parse.
[[nodiscard]] Results parse_output(std::string_view text);

/// Partition every comparison by whether it can affect a drag calculation (see the .cpp).  Throws BadInput if `single` lacks a key of `dbl`.
[[nodiscard]] Classes classify(const Results& single, const Results& dbl, double material = kMaterial);
/// "published case 4" or "sweep alt 240.01 km, condition (200,43200,45,90,100,90,15)": a comparison's place, computed from the sweep's own loop order.
[[nodiscard]] std::string describe(const Key& key);
/// The generated header.  `compiler` is the first line of the compiler's --version.
[[nodiscard]] std::string emit(const std::string& compiler, const std::string& sha256_of_source, const Classes& classes, const Results& dbl, const double (&sensitivity)[3]);

/// shutil.which(name) on `path` (a PATH-like string): the first directory that holds an executable regular file of that name; empty if none.
[[nodiscard]] std::string which(const std::string& name, const std::string& path);

struct Settings {
    std::filesystem::path root;
    std::filesystem::path work;                 // where the Fortran is built and run; empty: <root>/tools/.msisref
    std::optional<std::string> path;            // the PATH the compiler is looked for on; unset: the environment's
    std::vector<std::string> compilers = {"gfortran", "gfortran-16", "gfortran-15", "gfortran-14", "gfortran-13"};
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::msis_reference
