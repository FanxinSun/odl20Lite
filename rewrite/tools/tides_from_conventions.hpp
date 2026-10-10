#pragma once
// tools/tides_from_conventions.hpp — the tidal EOP coefficients, out of the document (SPEC-eop.md EOP-R-007, SPEC-perturbations PERT-R-014; plan L0 step 8, group C10).
//
// `run` is the whole tool: `tides_from_conventions --ch5 icc5.pdf --ch8 icc8.pdf --out HEADER [--ch6 icc6.pdf --out-ch6 HEADER6]`.  It runs the HOST PROGRAM `pdftotext -layout` on the pinned IERS Conventions
// chapters, reads the rows of the printed tables and writes them as generated source.  pdftotext is a regeneration-only host program (poppler, GPL): it is not part of the build, the tests or CI, it is not
// declared in the manifest and it is not redistributed (the maintainer's ruling D9); the tests use a stand-in.  Exit 0 written; 1 a table is missing or its row count is not the expected one, or the
// options contradict each other (the Python's sys.exit(message)); 2 an argument error or an input that is missing, unreadable or not what pdftotext can read; 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::tides_from_conventions {

inline constexpr char kTool[] = "tides_from_conventions";

/// What the Python ended with sys.exit(message): a table that is not found, a count that is not the expected one, options that go together.  Exit 1, the message alone on standard error.
struct Fatal : std::runtime_error {
    using std::runtime_error::runtime_error;
};
/// An input that is missing, unreadable or not the shape this reads (the Python's FileNotFoundError, CalledProcessError, UnicodeDecodeError): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// A row of Tables 5.1 and 8.2 to 8.3: the six multipliers, the period, the four coefficients — as floats, in that order.
using Row = std::array<double, 11>;
using Rows = std::vector<Row>;

/// Python's  re.match(r"^[+-]?(\d+\.?\d*|\.\d+)$", token):  is it a number the tables print (ASCII or any other decimal digits, an optional sign, an optional point, no exponent)?
[[nodiscard]] bool is_numeric(std::string_view token);

/// float(token) for a token that is_numeric (decimal digits of any script are read as their value, as Python's float() does).
[[nodiscard]] double numeric_value(std::string_view token);

/// pdftotext's standard output as the lines the Python worked on: universal newlines, U+2212 (the PDF's minus sign) written as an ASCII hyphen, then str.splitlines() (a form feed ends a line, too).
[[nodiscard]] std::vector<std::string> to_lines(std::string_view pdftotext_output);

/// Every row between the caption that begins with `start_marker` and the next caption, the fields taken FROM THE END of the row: ... <6 multipliers> <Doodson> <period/days> <c1> <c2> <c3> <c4>.
/// Throws Fatal("table 'MARKER' not found").
[[nodiscard]] Rows table_rows(const std::vector<std::string>& lines, std::string_view start_marker, const std::vector<std::string>& stop_markers);

/// The columns that follow the Delaunay multipliers in a row of Tables 6.5a/b/c, in the order the table prints them.
enum class Column { DkReal, DkImag, AmpIp, AmpOp };

struct Ch6Spec {
    std::string name;                   // kSolidTideDiurnal ...
    std::string start;                  // the caption that begins the table
    std::vector<std::string> stop;      // the captions that end it
    std::vector<Column> tail;
    double dk_scale;
    std::string what;
};
/// The three tables of chapter 6, in the order the tool writes them.
[[nodiscard]] const std::vector<Ch6Spec>& ch6_specs();

struct Ch6Row {
    double deg_per_hour = 0.0;
    std::array<int, 6> doodson{};
    std::array<int, 5> delaunay{};
    double dk_real = 0.0;
    double dk_imag = 0.0;   // zero where the table has no such column
    double amp_ip = 0.0;
    double amp_op = 0.0;    // zero where the table has no such column
};

/// The rows of one table of chapter 6.  Throws Fatal("table 'MARKER' not found").
[[nodiscard]] std::vector<Ch6Row> ch6_rows(const std::vector<std::string>& lines, const Ch6Spec& spec);

/// "name: N of M rows constrain it (K lost to the printed precision), worst residual W of its rounding bound at D deg/hr" (the line written to standard error).
[[nodiscard]] std::string ch6_consistency(const std::vector<Ch6Row>& rows, const std::string& name);

/// The text of one table of the header of chapter 6 / of chapters 5 and 8 (no trailing newline).
[[nodiscard]] std::string emit_ch6(const std::vector<Ch6Row>& rows, const std::string& name, const Ch6Spec& spec);
[[nodiscard]] std::string emit(const Rows& rows, const std::string& name, const std::string& unit, const std::string& what);

/// The whole generated header of chapter 6 (solid_tide_tables.hpp) and of chapters 5 and 8 (tide_tables.hpp).
[[nodiscard]] std::string ch6_header(const std::string& sha256_of_pdf, const std::vector<std::vector<Ch6Row>>& tables);
struct NamedRows {
    std::string name;
    Rows rows;
};
[[nodiscard]] std::string tide_header(const std::string& sha256_ch5, const std::string& sha256_ch8, const std::vector<NamedRows>& tables);

struct Settings {
    /// The program that turns a PDF into text; `program -layout PDF -` is run.  The tests put a stand-in here.
    std::string program = "pdftotext";
    /// If set, called instead of running the program: the standard output of `pdftotext -layout PDF -`.  The tests' seam for the logic.
    std::function<std::string(const std::filesystem::path&)> pdftotext;
};
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::tides_from_conventions
