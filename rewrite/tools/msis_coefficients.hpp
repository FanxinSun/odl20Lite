#pragma once
// tools/msis_coefficients.hpp — NRLMSISE-00's fitted coefficients, extracted from the pinned Fortran as generated source (SPEC-atmosphere ATMO-R-001/ATMO-R-002; plan L0 step 8, group C10).
//
// `run` is the whole tool: `msis_coefficients [--root DIR] [--out HEADER] [--check]`.  It reads the manifest entry `nrlmsise00-fortran` from the cache, verifies its hash, extracts the 3,300 coefficients
// of the DATA statements through the aliasing of COMMON/PARM7/ (see the .cpp) and writes modules/atmosphere/src/msis_coefficients.hpp, or with --check says whether the committed file is what the
// extraction emits.  Exit 0 written / the committed file reproduces; 1 the committed file differs, or the Fortran is not the shape the extraction expects (the Python's sys.exit(message));
// 2 an argument error or an input that is missing or cannot be read; 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odl::tools::msis_coefficients {

inline constexpr char kTool[] = "msis_coefficients";
inline constexpr char kSourceId[] = "nrlmsise00-fortran";
inline constexpr char kDefaultOut[] = "modules/atmosphere/src/msis_coefficients.hpp";

/// What the Python ended with sys.exit(message): the Fortran does not have the shape the extraction expects, or the cached bytes are not the pinned bytes.  Exit 1, the message alone on standard error.
struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/// An input that is missing, unreadable or not the shape this reads (the Python's OSError, KeyError, StopIteration): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The tokens that Python's  re.compile(r"[-+]?\d*\.?\d+(?:[EeDd][-+]?\d+)?").findall  finds in `s`, leftmost and without overlap (ASCII digits: the pinned file is ASCII).
[[nodiscard]] std::vector<std::string_view> find_numbers(std::string_view s);

/// The numbers of the first statement  `      DATA NAME/ ... /`  that begins a line of `text` (six blanks, DATA, one blank, the name, the slash): every number between that slash and the next one, with
/// D and d exponents read as E.  Throws Fatal("no DATA statement for NAME") if there is none.
[[nodiscard]] std::vector<double> read_data(std::string_view text, std::string_view name);

/// One extracted block: `values` in storage order; n elements per column, m columns (m == 0: one-dimensional).
struct Block {
    std::vector<double> values;
    std::size_t n = 0;
    std::size_t m = 0;
};
using Blocks = std::vector<std::pair<std::string, Block>>;

/// The 64 DATA arrays of COMMON/PARM7/ in the order BLOCK DATA GTD7BK declares them.
[[nodiscard]] const std::vector<std::string>& parm7_names();

/// The ten blocks in the order of the generated file: the seven views of PARM7's 3,200 doubles (pt, pd, ps, pdl, ptl, pma, sam) and the three flat tables (ptm, pdm, pavgm).  Throws Fatal.
[[nodiscard]] Blocks extract(std::string_view fortran);

/// format(v, " .6e"): a space where a plus sign would be.
[[nodiscard]] std::string format_value(double v);

/// Five values to a row, each row indented by `indent` blanks and ending in a comma; the last row may be shorter.
[[nodiscard]] std::string format_values(const std::vector<double>& values, std::size_t indent = 4);

/// The generated header (every line ends in "\n").
[[nodiscard]] std::string emit(std::string_view sha256_of_source, const Blocks& blocks, std::size_t ndata);

struct Settings {
    std::filesystem::path root;
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::msis_coefficients
