// tides_from_conventions.cpp — the tidal EOP coefficients, out of the document.
//
// Plan L0 step 8, group C10, ported to C++ (the user's directive of 2026-10-06) from tools/tides_from_conventions.py (deleted by the same commit: `git show fb4b960:rewrite/tools/tides_from_conventions.py`).
//
// SPEC-eop.md EOP-R-007.  The IERS Conventions publish these models both as printed tables and as Fortran routines.  The Fortran carries NO LICENCE AT ALL, so there is nothing for plan §5 constraint 3 to
// be satisfied by; the tables are the content of a standard, published so that they can be implemented.  This tree therefore implements from the tables.  Hand transcription of ~1400 numbers would be its
// own defect source, so the tables are extracted from the hash-pinned PDFs by this tool and the result is COMMITTED as generated source.  The extraction is not part of the build: pdftotext output varies
// with the poppler version, and a build that re-ran it would not be reproducible.  What the build checks instead is EOP-R-008 — that the coefficients reproduce the routines' own published test cases,
// which is a check on the numbers rather than on the pipeline that produced them.
//
// Row shape, common to all six tables of chapters 5 and 8 and the reason a single parser handles them: each row ends with
//     ... <6 integer multipliers> <Doodson> <period/days> <c1> <c2> <c3> <c4>
// so the fields are taken FROM THE END.  That skips the optional tide-name column (Q1, 2Q1, χ1 …) and Table 5.1's extra leading degree column without having to model either.
//
// HOST PROGRAM.  This tool spawns `pdftotext -layout` (poppler 26.01.0 here; GPL, so it is not an entry of the manifest and is not redistributed — the maintainer's ruling D9).  It is needed only where the
// tables are REGENERATED: no ctest, no gate of ci.sh and nothing in the build runs it, and the tool's own tests use a stand-in program.  The committed headers were made on 2026-09-18 with a poppler
// version that they do not record.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C10_proof_registration.txt in the group's report files).  Its products are two committed texts,
// modules/eop/src/tide_tables.hpp (19,327 bytes, 230 lines) and modules/tides/src/solid_tide_tables.hpp (11,570 bytes, 129 lines), each naming the generator on line 4; the registered comparison is byte for
// byte but for that name, and the registered stop rule is that a difference is first looked for in the EXTRACTED NUMBER ROWS (the poppler version) and reported before anything is concluded about the port.
// C10_proof_result.txt holds the result as it came.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The Python's regular expression NUMERIC and its split() and strip() are the scanners below, with Python's whitespace and decimal digits (devkit is_py_space / is_py_decimal).
//   * What the Python ended with SystemExit(message) ends with the same message and exit 1 (Fatal); what was a traceback there (pdftotext missing or failing, text that is not UTF-8, an output path that
//     cannot be written) is a refusal naming the fault, exit 2 (BadInput).  An error the tool did not anticipate exits 70.
//   * The options are taken by their exact names (argparse took any unambiguous abbreviation); `-h` prints this tool's own text.
//   * CH6_TABLES of the Python is ch6_specs() here; the generated header still says "see CH6_TABLES in the generator", because those are the committed bytes.

#include "tides_from_conventions.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/process.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/text.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <utility>

namespace dk = odl::devkit;
using odl::devkit::Streams;

namespace odl::tools::tides_from_conventions {

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;

// the PDF uses U+2212, not ASCII hyphen
constexpr char kMinus[] = "\xE2\x88\x92";

// "%*.*f" and "%*d" as Python's format specs wrote them: right-aligned in the width
std::string fmt_f(double v, int width, int precision) { return dk::pad_left(dk::py_format_f(v, precision), static_cast<std::size_t>(width)); }
std::string fmt_d(long long v, int width) { return dk::pad_left(std::to_string(v), static_cast<std::size_t>(width)); }

// f"{int(a):3d}" for a float that holds an integer (int() truncates; the sign of a zero is lost)
std::string fmt_int_of(double a, int width) {
    double t = std::trunc(a);
    if (t == 0.0) t = 0.0;   // -0.0 is the integer 0
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.0f", t);
    return dk::pad_left(buf, static_cast<std::size_t>(width));
}

bool starts_with(std::string_view s, std::string_view prefix) { return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0; }

// the tokens of a stripped line that are numbers
std::vector<std::string> numeric_tokens(std::string_view stripped) {
    std::vector<std::string> out;
    for (std::string& t : dk::split_py(stripped)) {
        if (is_numeric(t)) out.push_back(std::move(t));
    }
    return out;
}

// the index of the first line whose stripped text begins with `marker`; Fatal if there is none ("table 'MARKER' not found": the message is the Python's, quotes by repr)
std::size_t find_caption(const std::vector<std::string>& lines, std::string_view marker) {
    for (std::size_t n = 0; n < lines.size(); ++n) {
        if (starts_with(dk::strip_py(lines[n]), marker)) return n;
    }
    throw Fatal("table " + dk::py_repr(marker) + " not found");
}

bool stops_here(std::string_view stripped, const std::vector<std::string>& stop_markers) {
    return std::any_of(stop_markers.begin(), stop_markers.end(), [&](const std::string& m) { return starts_with(stripped, m); });
}

// the values of an integer-valued float, or none
bool is_integral(double x) { return std::isfinite(x) && x == std::trunc(x); }

constexpr double kAmplitudeHalfUlp = 0.05;

}  // namespace

bool is_numeric(std::string_view token) {
    // ^[+-]?(\d+\.?\d*|\.\d+)$
    std::size_t i = 0;
    std::uint32_t cp = 0;
    std::size_t after = 0;
    if (token.empty()) return false;
    if (token[0] == '+' || token[0] == '-') i = 1;
    std::size_t digits_before = 0;
    while (i < token.size()) {
        cp = dk::code_point_at(token, i, after);
        if (!dk::is_py_decimal(cp)) break;
        ++digits_before;
        i = after;
    }
    if (digits_before > 0) {
        if (i < token.size() && token[i] == '.') ++i;
        while (i < token.size()) {
            cp = dk::code_point_at(token, i, after);
            if (!dk::is_py_decimal(cp)) return false;
            i = after;
        }
        return true;
    }
    // \.\d+
    if (i >= token.size() || token[i] != '.') return false;
    ++i;
    std::size_t digits_after = 0;
    while (i < token.size()) {
        cp = dk::code_point_at(token, i, after);
        if (!dk::is_py_decimal(cp)) return false;
        ++digits_after;
        i = after;
    }
    return digits_after > 0;
}

double numeric_value(std::string_view token) {
    std::string ascii;
    std::size_t i = 0;
    while (i < token.size()) {
        std::size_t after = 0;
        const std::uint32_t cp = dk::code_point_at(token, i, after);
        const int value = dk::py_decimal_value(cp);
        if (value >= 0) {
            ascii += static_cast<char>('0' + value);
        } else {
            ascii.append(token.substr(i, after - i));
        }
        i = after;
    }
    return std::strtod(ascii.c_str(), nullptr);
}

std::vector<std::string> to_lines(std::string_view pdftotext_output) {
    std::string text = dk::universal_newlines(pdftotext_output);
    std::string replaced;
    replaced.reserve(text.size());
    for (std::size_t i = 0; i < text.size();) {
        if (text.compare(i, 3, kMinus) == 0) {
            replaced += '-';
            i += 3;
        } else {
            replaced += text[i++];
        }
    }
    return dk::splitlines_py(replaced);
}

Rows table_rows(const std::vector<std::string>& lines, std::string_view start_marker, const std::vector<std::string>& stop_markers) {
    const std::size_t i = find_caption(lines, start_marker);
    Rows rows;
    for (std::size_t n = i + 1; n < lines.size(); ++n) {
        const std::string s = dk::strip_py(lines[n]);
        if (stops_here(s, stop_markers)) break;
        const std::vector<std::string> toks = numeric_tokens(s);
        if (toks.size() < 12) continue;
        const std::size_t base = toks.size() - 12;   // tail = toks[-12:]
        Row row{};
        for (std::size_t k = 0; k < 6; ++k) {
            const double v = numeric_value(toks[base + k]);
            // the Python's int(float(x)) raises OverflowError for an infinity: not a ValueError, so a traceback; here the refusal names the token
            if (!std::isfinite(v)) throw BadInput("a number in a table is too large: " + toks[base + k]);
            row[k] = std::trunc(v);
        }
        const std::string& doodson = toks[base + 6];
        const double period = numeric_value(toks[base + 7]);
        for (std::size_t k = 0; k < 4; ++k) row[7 + k] = numeric_value(toks[base + 8 + k]);
        if (doodson.find('.') == std::string::npos || period <= 0.0) continue;   // a stray line, not a constituent
        row[6] = period;
        rows.push_back(row);
    }
    return rows;
}

const std::vector<Ch6Spec>& ch6_specs() {
    static const std::vector<Ch6Spec> specs = {
        {"kSolidTideDiurnal", "Table 6.5a:", {"Table 6.5b:"}, {Column::DkReal, Column::DkImag, Column::AmpIp, Column::AmpOp}, 1e-5,
         "Table 6.5a - diurnal (m = 1) corrections for the frequency dependence of k21"},
        {"kSolidTideZonal", "Table 6.5b:", {"Table 6.5c:"}, {Column::DkReal, Column::AmpIp, Column::DkImag, Column::AmpOp}, 1.0,
         "Table 6.5b - zonal (m = 0) corrections for the frequency dependence of k20"},
        {"kSolidTideSemidiurnal", "Table 6.5c:", {"6.2.2"}, {Column::DkReal, Column::AmpIp}, 1.0, "Table 6.5c - semidiurnal (m = 2) corrections for k22; the real part only"},
    };
    return specs;
}

std::vector<Ch6Row> ch6_rows(const std::vector<std::string>& lines, const Ch6Spec& spec) {
    const std::size_t want = 1 + 6 + 5 + spec.tail.size();   // deg/hr, Doodson multipliers, Delaunay, tail
    const std::size_t i = find_caption(lines, spec.start);
    std::vector<Ch6Row> rows;
    for (std::size_t n = i + 1; n < lines.size(); ++n) {
        const std::string s = dk::strip_py(lines[n]);
        if (stops_here(s, spec.stop)) break;
        const std::vector<std::string> toks = numeric_tokens(s);
        if (toks.size() != want) continue;   // a caption, a header, a page number
        std::vector<double> v;
        v.reserve(toks.size());
        for (const std::string& t : toks) v.push_back(numeric_value(t));
        if (!(0.0 < v[0] && v[0] < 40.0)) continue;   // deg/hr for every tide in these bands
        bool multipliers_ok = true;
        for (std::size_t k = 1; k < 12; ++k) {
            if (std::fabs(v[k]) > 20 || !is_integral(v[k])) multipliers_ok = false;   // the multipliers are small integers
        }
        if (!multipliers_ok) continue;
        Ch6Row row;
        row.deg_per_hour = v[0];
        for (std::size_t k = 0; k < 6; ++k) row.doodson.at(k) = static_cast<int>(v[1 + k]);
        for (std::size_t k = 0; k < 5; ++k) row.delaunay.at(k) = static_cast<int>(v[7 + k]);
        for (std::size_t k = 0; k < spec.tail.size(); ++k) {
            const double x = v[12 + k];
            switch (spec.tail[k]) {
                case Column::DkReal: row.dk_real = x; break;
                case Column::DkImag: row.dk_imag = x; break;
                case Column::AmpIp: row.amp_ip = x; break;
                case Column::AmpOp: row.amp_op = x; break;
            }
        }
        rows.push_back(row);
    }
    return rows;
}

std::string ch6_consistency(const std::vector<Ch6Row>& rows, const std::string& name) {
    double worst = 0.0;
    const Ch6Row* worst_row = nullptr;
    std::size_t constraining = 0;
    std::size_t applicable = 0;
    for (const Ch6Row& r : rows) {
        if (r.dk_imag == 0.0 && r.amp_op == 0.0) continue;   // the table has no such column
        ++applicable;
        const double residual = r.amp_ip * r.dk_imag - r.amp_op * r.dk_real;
        const double bound = kAmplitudeHalfUlp * (std::fabs(r.dk_imag) + std::fabs(r.dk_real));
        const double terms = std::max(std::fabs(r.amp_ip * r.dk_imag), std::fabs(r.amp_op * r.dk_real));
        if (terms <= bound) continue;   // constrains nothing
        ++constraining;
        const double d = bound != 0.0 ? std::fabs(residual) / bound : 0.0;
        if (d > worst) {
            worst = d;
            worst_row = &r;
        }
    }
    if (applicable == 0) return name + ": the table has no imaginary column; the relation does not apply";
    std::string out = name + ": " + std::to_string(constraining) + " of " + std::to_string(applicable) + " rows constrain it (" + std::to_string(applicable - constraining) +
                      " lost to the printed precision), worst residual " + dk::py_format_f(worst, 2) + " of its rounding bound";
    if (worst_row != nullptr) out += " at " + dk::py_float_repr(worst_row->deg_per_hour) + " deg/hr";
    return out;
}

std::string emit_ch6(const std::vector<Ch6Row>& rows, const std::string& name, const Ch6Spec& spec) {
    std::string out = "// " + spec.what + "\n";
    out += "//   " + std::to_string(rows.size()) + " constituents; dk as printed (scale " + dk::py_format_g(spec.dk_scale) + "), amplitudes as printed (scale 1e-12)\n";
    out += "inline constexpr SolidTideTerm " + name + "[] = {\n";
    for (const Ch6Row& r : rows) {
        std::string d;
        for (std::size_t k = 0; k < r.doodson.size(); ++k) d += (k != 0 ? ", " : "") + fmt_d(r.doodson[k], 3);
        std::string l;
        for (std::size_t k = 0; k < r.delaunay.size(); ++k) l += (k != 0 ? ", " : "") + fmt_d(r.delaunay[k], 3);
        out += "    {" + fmt_f(r.deg_per_hour, 11, 5) + ", {" + d + "}, {" + l + "}, " + fmt_f(r.dk_real, 12, 5) + ", " + fmt_f(r.dk_imag, 12, 5) + ", " + fmt_f(r.amp_ip, 8, 1) + ", " +
               fmt_f(r.amp_op, 8, 1) + "},\n";
    }
    out += "};";
    return out;
}

std::string emit(const Rows& rows, const std::string& name, const std::string& unit, const std::string& what) {
    std::string out = "// " + what + "  (" + std::to_string(rows.size()) + " constituents, " + unit + ")\n";
    out += "inline constexpr TideTerm " + name + "[] = {\n";
    for (const Row& r : rows) {
        std::string args;
        for (std::size_t k = 0; k < 6; ++k) args += (k != 0 ? ", " : "") + fmt_int_of(r[k], 3);
        std::string c;
        for (std::size_t k = 7; k < 11; ++k) c += (k != 7 ? ", " : "") + fmt_f(r[k], 9, 4);
        out += "    {{" + args + "}, " + fmt_f(r[6], 12, 7) + ", " + c + "},\n";
    }
    out += "};";
    return out;
}

std::string ch6_header(const std::string& sha256_of_pdf, const std::vector<std::vector<Ch6Row>>& tables) {
    const std::vector<Ch6Spec>& specs = ch6_specs();
    std::string text =
        "#pragma once\n"
        "// solid_tide_tables.hpp — GENERATED.  Do not edit.\n"
        "//\n"
        "// Produced by tools/tides_from_conventions.cpp --ch6 from the hash-pinned IERS\n"
        "// Conventions (2010) chapter 6:  sha256 " + sha256_of_pdf + "\n"
        "//\n"
        "// SPEC-perturbations PERT-R-014: Step 2's frequency-dependent corrections are\n"
        "// implemented from the tables PRINTED IN THE CONVENTIONS.  Several hundred\n"
        "// numbers is its own defect source by hand, and the IERS Fortran carries no\n"
        "// licence at all, so neither route is open.  Extraction is NOT part of the\n"
        "// build: pdftotext's layout varies with the poppler version and a build that\n"
        "// re-ran it would not be reproducible.\n"
        "//\n"
        "// VALUES ARE AS PRINTED so that this file can be diffed against the PDF.  The\n"
        "// scales are named below and applied once, in the module.\n"
        "//\n"
        "// The tail columns differ between the three tables and 6.5a's Love-number\n"
        "// corrections are in units of 1e-5 where 6.5b's and 6.5c's are absolute; see\n"
        "// CH6_TABLES in the generator.\n"
        "\n"
        "#include <array>\n"
        "\n"
        "namespace odl::tides::tables {\n"
        "\n"
        "struct SolidTideTerm {\n"
        "    double deg_per_hour;\n"
        "    std::array<int, 6> doodson;    // tau, s, h, p, N', ps\n"
        "    std::array<int, 5> delaunay;   // l, l', F, D, Omega\n"
        "    double dk_real;                // AS PRINTED\n"
        "    double dk_imag;                // AS PRINTED; zero where the table has no such column\n"
        "    double amp_ip;                 // AS PRINTED, units of 1e-12\n"
        "    double amp_op;                 // AS PRINTED, units of 1e-12; zero where absent\n"
        "};\n"
        "\n"
        "/// TN36-6 Table 6.5a prints dk in units of 1e-5.  Tables 6.5b and 6.5c print it\n"
        "/// absolute.  One number per table, applied once, named here.\n"
        "inline constexpr double kDkScaleDiurnal = " + dk::py_format_g(specs[0].dk_scale) + ";\n"
        "inline constexpr double kDkScaleZonal = " + dk::py_format_g(specs[1].dk_scale) + ";\n"
        "inline constexpr double kDkScaleSemidiurnal = " + dk::py_format_g(specs[2].dk_scale) + ";\n"
        "/// Every amplitude column in all three tables is in units of 1e-12.\n"
        "inline constexpr double kAmplitudeScale = 1e-12;\n"
        "\n";
    for (std::size_t k = 0; k < specs.size(); ++k) {
        text += emit_ch6(tables.at(k), specs[k].name, specs[k]);
        text += "\n\n";
    }
    text += "}  // namespace odl::tides::tables\n";
    return text;
}

std::string tide_header(const std::string& sha256_ch5, const std::string& sha256_ch8, const std::vector<NamedRows>& tables) {
    struct Caption {
        const char* name;
        const char* unit;
        const char* what;
    };
    static const Caption kCaptions[] = {
        {"kPoleOceanDiurnal", "microarcseconds", "Table 8.2a - diurnal ocean-tide variations in pole coordinates (xp sin, xp cos, yp sin, yp cos)"},
        {"kPoleOceanSemidiurnal", "microarcseconds", "Table 8.2b - semidiurnal ocean-tide variations in pole coordinates"},
        {"kUt1OceanDiurnal", "microseconds / microseconds per day", "Table 8.3a - diurnal ocean-tide variations in UT1 and LOD (UT1 sin, UT1 cos, LOD sin, LOD cos)"},
        {"kUt1OceanSemidiurnal", "microseconds / microseconds per day", "Table 8.3b - semidiurnal ocean-tide variations in UT1 and LOD"},
        {"kPoleLibration", "microarcseconds",
         "Table 5.1a - libration in pole coordinates. ONLY the near-diurnal rows are applied: TN36 5.5.1.1 says the long-period terms are already in the observed series"},
        {"kUt1Libration", "microseconds / microseconds per day", "Table 5.1b - semidiurnal libration in UT1 and LOD"},
    };
    std::string text =
        "#pragma once\n"
        "// tide_tables.hpp — GENERATED.  Do not edit.\n"
        "//\n"
        "// Produced by tools/tides_from_conventions.cpp from the hash-pinned IERS\n"
        "// Conventions (2010) chapters in the manifest:\n"
        "//\n"
        "//   chapter 5  sha256 " + sha256_ch5 + "\n"
        "//   chapter 8  sha256 " + sha256_ch8 + "\n"
        "//\n"
        "// SPEC-eop.md EOP-R-007: the models are implemented from the tables PRINTED IN\n"
        "// THE CONVENTIONS, not from the IERS Fortran, which carries no licence.\n"
        "// EOP-R-008: every table here is verified against that routine's own published\n"
        "// test case, and a transcription that fails its test case is a build failure.\n"
        "//\n"
        "// The six multipliers are of (gamma, l, l', F, D, Omega), where gamma = GMST + pi.\n"
        "\n"
        "#include <array>\n"
        "\n"
        "namespace odl::eop::tides {\n"
        "\n"
        "struct TideTerm {\n"
        "    std::array<int, 6> arg;\n"
        "    double period_days;   // carried as DATA, not a comment: Table 5.1a is filtered\n"
        "                          // to its near-diurnal rows and the filter needs it\n"
        "    double sin1, cos1, sin2, cos2;\n"
        "};\n"
        "\n";
    for (const Caption& c : kCaptions) {
        const auto it = std::find_if(tables.begin(), tables.end(), [&](const NamedRows& t) { return t.name == c.name; });
        if (it == tables.end()) throw std::logic_error(std::string("the table ") + c.name + " was not given");
        text += emit(it->rows, c.name, c.unit, c.what);
        text += "\n\n";
    }
    text += "}  // namespace odl::eop::tides\n";
    return text;
}

Settings default_settings() { return Settings{}; }

namespace {

const char kUsageText[] = "usage: tides_from_conventions [-h] --ch5 CH5 --ch8 CH8 --out OUT [--ch6 CH6] [--out-ch6 OUT_CH6]\n";

const char kHelpText[] =
    "\n"
    "The tidal EOP coefficients, out of the document: the tables of chapters 5, 6 and 8 of the IERS Conventions (2010), read from the pinned PDFs through `pdftotext -layout` (a regeneration-only host program) and\n"
    "written as generated source.  Usage: tides_from_conventions --ch5 icc5.pdf --ch8 icc8.pdf --out <header> [--ch6 icc6.pdf --out-ch6 <header>]\n"
    "\n"
    "options:\n"
    "  -h, --help          show this help and exit\n"
    "  --ch5 CH5           chapter 5 (Table 5.1: libration)\n"
    "  --ch8 CH8           chapter 8 (Tables 8.2 and 8.3: ocean tides)\n"
    "  --out OUT           the header for chapters 5 and 8 (modules/eop/src/tide_tables.hpp)\n"
    "  --ch6 CH6           IERS Conventions chapter 6, for Tables 6.5a/b/c\n"
    "  --out-ch6 OUT_CH6   where to write the solid Earth tide tables (modules/tides/src/solid_tide_tables.hpp); goes with --ch6\n"
    "\n"
    "exit codes: 0 written   1 a table is not found or its row count is not the expected one, or --ch6 and --out-ch6 do not go together   2 an argument error, or pdftotext or an input or output path failed\n"
    "            70 an error the tool did not anticipate\n";

// pathlib's str(Path(p)): no empty and no "." components, no trailing slash; "." for what is left of nothing.
std::string path_text(const std::string& given) {
    std::string out;
    const bool absolute = !given.empty() && given.front() == '/';
    std::size_t i = 0;
    while (i <= given.size()) {
        const std::size_t slash = std::min(given.find('/', i), given.size());
        const std::string part = given.substr(i, slash - i);
        if (!part.empty() && part != ".") {
            if (!out.empty() || absolute) out += '/';
            out += part;
        }
        i = slash + 1;
    }
    if (out.empty()) return absolute ? "/" : ".";
    return out;
}

// subprocess.run([program, "-layout", pdf, "-"], capture_output=True, text=True, check=True).stdout
std::string run_pdftotext(const Settings& settings, const std::filesystem::path& pdf) {
    if (settings.pdftotext) return settings.pdftotext(pdf);
    dk::ProcessResult r;
    try {
        r = dk::run_process({settings.program, "-layout", pdf.string(), "-"});
    } catch (const std::runtime_error& exc) {
        throw BadInput(std::string("cannot run ") + settings.program + ": " + exc.what());
    }
    if (r.exit_code != 0) {
        std::string tail = r.err.size() > 500 ? r.err.substr(r.err.size() - 500) : r.err;
        throw BadInput(settings.program + " -layout " + pdf.string() + " - exited with status " + std::to_string(r.exit_code) + (tail.empty() ? "" : ": " + tail));
    }
    if (!dk::valid_utf8(r.out)) throw BadInput(settings.program + " -layout " + pdf.string() + " - wrote text that is not UTF-8");
    return r.out;
}

std::string sha256_of(const std::filesystem::path& file) { return dk::sha256_hex(dk::read_bytes(file)); }

void write_with_parents(const std::filesystem::path& out, const std::string& text) {
    const std::filesystem::path parent = out.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) throw BadInput(parent.string() + ": " + ec.message());
    }
    dk::write_text(out, text);
}

void write_ch6(const Settings& settings, const std::filesystem::path& pdf, const std::string& out_given, Streams io) {
    const std::vector<std::string> lines = to_lines(run_pdftotext(settings, pdf));
    std::vector<std::vector<Ch6Row>> tables;
    for (const Ch6Spec& spec : ch6_specs()) tables.push_back(ch6_rows(lines, spec));
    for (std::size_t k = 0; k < tables.size(); ++k) {
        const std::string& name = ch6_specs()[k].name;
        io.err << "extracted " << name << ": " << tables[k].size() << " constituents\n";
        io.err << "  " << ch6_consistency(tables[k], name) << '\n';
    }
    // The counts are asserted, not reported: a shape change upstream that silently dropped half a table would otherwise pass as "extracted 35".
    // Counted from the PDF, not guessed: 48 + 21 + 2 = 71 constituents in all.
    const std::size_t expect[] = {48, 21, 2};
    for (std::size_t k = 0; k < tables.size(); ++k) {
        if (tables[k].size() != expect[k]) {
            throw Fatal(ch6_specs()[k].name + ": extracted " + std::to_string(tables[k].size()) + " constituents, expected " + std::to_string(expect[k]) +
                        ".\n"
                        "  The table's shape in the PDF has changed, or pdftotext laid it out\n"
                        "  differently. Re-read Tables 6.5a/b/c before touching this number:\n"
                        "  the three have three different column orders and 6.5a's dk is in\n"
                        "  units of 1e-5 where the other two are absolute.");
        }
    }
    const std::string text = ch6_header(sha256_of(pdf), tables);
    write_with_parents(out_given, text);
    std::size_t total = 0;
    for (const auto& t : tables) total += t.size();
    io.err << "wrote " << path_text(out_given) << " (" << total << " constituents)\n";
}

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        std::optional<std::string> ch5, ch8, out, ch6, out_ch6;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--ch5" || opt == "--ch8" || opt == "--out" || opt == "--ch6" || opt == "--out-ch6") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--ch5") ch5 = value;
                else if (opt == "--ch8") ch8 = value;
                else if (opt == "--out") out = value;
                else if (opt == "--ch6") ch6 = value;
                else out_ch6 = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }
        std::string missing;
        if (!ch5) missing += (missing.empty() ? "" : ", ") + std::string("--ch5");
        if (!ch8) missing += (missing.empty() ? "" : ", ") + std::string("--ch8");
        if (!out) missing += (missing.empty() ? "" : ", ") + std::string("--out");
        if (!missing.empty()) return usage_error("the following arguments are required: " + missing);

        try {
            if (ch6.has_value() != out_ch6.has_value()) throw Fatal("--ch6 and --out-ch6 go together");
            if (ch6) write_ch6(settings, *ch6, *out_ch6, io);

            const std::vector<std::string> c5 = to_lines(run_pdftotext(settings, *ch5));
            const std::vector<std::string> c8 = to_lines(run_pdftotext(settings, *ch8));

            const std::vector<NamedRows> tables = {
                {"kPoleOceanDiurnal", table_rows(c8, "Table 8.2a:", {"Table 8.2b:"})},
                {"kPoleOceanSemidiurnal", table_rows(c8, "Table 8.2b:", {"Table 8.3a:"})},
                {"kUt1OceanDiurnal", table_rows(c8, "Table 8.3a:", {"Table 8.3b:"})},
                {"kUt1OceanSemidiurnal", table_rows(c8, "Table 8.3b:", {"Table 8.4", "8.3.2", "Table 8.5"})},
                {"kPoleLibration", table_rows(c5, "Table 5.1a:", {"Table 5.1b:", "5.5.2"})},
                {"kUt1Libration", table_rows(c5, "Table 5.1b:", {"5.5.4", "Table 5.2"})},
            };
            std::size_t total = 0;
            io.err << "extracted: {";
            for (std::size_t k = 0; k < tables.size(); ++k) {
                io.err << (k != 0 ? ", " : "") << '\'' << tables[k].name << "': " << tables[k].rows.size();
                total += tables[k].rows.size();
            }
            io.err << "}\n";

            const std::string header = tide_header(sha256_of(*ch5), sha256_of(*ch8), tables);
            write_with_parents(*out, header);
            io.err << "wrote " << path_text(*out) << " (" << total << " constituents)\n";
            return kOk;
        } catch (const Fatal& exc) {
            io.err << exc.what() << '\n';
            return kFailed;
        } catch (const BadInput& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        } catch (const std::runtime_error& exc) {   // dk::read_bytes, write_text: the path and the reason
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::tides_from_conventions

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::tides_from_conventions::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
