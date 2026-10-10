// msis_coefficients.cpp — extract NRLMSISE-00's fitted coefficients as generated source.
//
// Plan L0 step 8, group C10, ported to C++ (the user's directive of 2026-10-06) from tools/msis_coefficients.py (deleted by the same commit: `git show fb4b960:rewrite/tools/msis_coefficients.py`).
//
// SPEC-atmosphere ATMO-R-001/ATMO-R-002.  The model IS ~1500 fitted coefficients plus the code combining them, and both exist only in the pinned FORTRAN (plan §4 rule 6).  The coefficients are therefore
// EXTRACTED from the pinned bytes, never transcribed: there are 3 300 of them and hand-copying would be its own defect source, exactly as PERT-R-014 reasoned about chapter 6's tide tables.
// Extraction is NOT part of the build.  It runs against the manifest cache and its output is committed, so a build reproduces without re-parsing anything; the ctest
// `atmosphere.msis_coefficients_match_generator` runs `--check` on the real tree (the cache and the committed header, no host program).
//
// THE STORAGE IS FORTRAN COMMON ALIASING, which is the one subtle thing here.  BLOCK DATA GTD7BK declares COMMON/PARM7/ as sixty-four 50-element arrays PT1..PAA2 (3 200 doubles).  Every subroutine declares
// the SAME common block as PT(150), PD(150,9), PS(150), PDL(25,2), PTL(100,4), PMA(100,10), SAM(100) — 150 + 1350 + 150 + 50 + 400 + 1000 + 100 = 3 200.  So the DATA statements fill a flat block that the
// code reads through a completely different set of names, and an extractor that took the DATA names at face value would produce arrays the model never indexes.  The concatenation order is the DECLARATION
// order, and the two-dimensional views are COLUMN-MAJOR.
//
// THE PROOF OF THE PORT (registered before any line of this file was written: C10_proof_registration.txt in the group's report files).  The tool's product is one committed text,
// modules/atmosphere/src/msis_coefficients.hpp (56,562 bytes, 779 lines, sha256 cfe2f4c9...554d), and the port wrote it BYTE FOR BYTE on its first comparison but for the generator's name on line 4
// (".py" -> ".cpp"; 56,563 bytes) — the substitution list registered beforehand.  C10_proof_result.txt holds the result as it came.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The Python's regular expressions are hand-written scanners: the number pattern  [-+]?\d*\.?\d+(?:[EeDd][-+]?\d+)?  (find_numbers) with its backtracking written out (a "1." is the number 1 and a
//     lone "." is nothing), and the search  ^      DATA NAME/  (the first line of the text that begins with exactly that).  The pinned file is ASCII (checked: no byte above 0x7f), so \d is ASCII's.
//   * What the Python ended with sys.exit(message) ends with the same message and exit 1 (Fatal); what was a traceback there (the manifest or the cached file missing, a field of the manifest missing,
//     a statement with no closing slash) is a refusal naming the fault, exit 2 (BadInput).  An error the tool did not anticipate exits 70.
//   * The options are taken by their exact names (argparse took any unambiguous abbreviation); `-h` prints this tool's own text; `--root DIR` is new (the Python's tree was the one the script stood in; the
//     default here is the tree this tool was built from).

#include "msis_coefficients.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/sha256.hpp>

#include <cstdlib>
#include <iostream>

namespace dk = odl::devkit;
using odl::devkit::Streams;

namespace odl::tools::msis_coefficients {

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;

constexpr std::size_t kParm7Total = 3200;

bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

// how the subroutines view the common block: name, elements per column, columns (0: one-dimensional)
struct View {
    const char* name;
    std::size_t n;
    std::size_t m;
};
constexpr View kViews[] = {{"pt", 150, 0}, {"pd", 150, 9}, {"ps", 150, 0}, {"pdl", 25, 2}, {"ptl", 100, 4}, {"pma", 100, 10}, {"sam", 100, 0}};
// the tables that are not part of the common block
constexpr View kFlats[] = {{"PTM", 10, 0}, {"PDM", 10, 8}, {"PAVGM", 10, 0}};

}  // namespace

std::vector<std::string_view> find_numbers(std::string_view s) {
    std::vector<std::string_view> out;
    std::size_t p = 0;
    while (p < s.size()) {
        std::size_t i = p;
        if (s[i] == '+' || s[i] == '-') ++i;
        const std::size_t digits_begin = i;
        while (i < s.size() && is_digit(s[i])) ++i;
        std::size_t end = 0;   // the end of the mantissa, once there is one
        bool matched = false;
        if (i + 1 < s.size() && s[i] == '.' && is_digit(s[i + 1])) {
            // \d*  \.  \d+ : the digits, the point and at least one digit after it
            end = i + 1;
            while (end < s.size() && is_digit(s[end])) ++end;
            matched = true;
        } else if (i > digits_begin) {
            // \d+ alone: the greedy \d* gave back its last digit (a point not followed by a digit is not part of the number)
            end = i;
            matched = true;
        }
        if (!matched) {
            ++p;
            continue;
        }
        // (?:[EeDd][-+]?\d+)? : an exponent only if it is whole
        if (end < s.size() && (s[end] == 'E' || s[end] == 'e' || s[end] == 'D' || s[end] == 'd')) {
            std::size_t t = end + 1;
            if (t < s.size() && (s[t] == '+' || s[t] == '-')) ++t;
            const std::size_t exponent_digits = t;
            while (t < s.size() && is_digit(s[t])) ++t;
            if (t > exponent_digits) end = t;
        }
        out.push_back(s.substr(p, end - p));
        p = end;
    }
    return out;
}

std::vector<double> read_data(std::string_view text, std::string_view name) {
    const std::string needle = "      DATA " + std::string(name) + "/";
    std::size_t from = 0;
    std::size_t after = std::string_view::npos;
    for (;;) {
        const std::size_t at = text.find(needle, from);
        if (at == std::string_view::npos) throw Fatal("no DATA statement for " + std::string(name));
        if (at == 0 || text[at - 1] == '\n') {   // re.M: ^ is the start of a line
            after = at + needle.size();
            break;
        }
        from = at + 1;
    }
    const std::string_view body = text.substr(after);
    const std::size_t end = body.find('/');
    if (end == std::string_view::npos) throw BadInput("the DATA statement for " + std::string(name) + " has no closing slash");
    std::vector<double> out;
    for (const std::string_view token : find_numbers(body.substr(0, end))) {
        std::string t(token);
        for (char& c : t) {
            if (c == 'D') c = 'E';
            else if (c == 'd') c = 'e';
        }
        out.push_back(std::strtod(t.c_str(), nullptr));
    }
    return out;
}

const std::vector<std::string>& parm7_names() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> v;
        for (int i : {1, 2, 3}) v.push_back("PT" + std::to_string(i));
        for (const char c : std::string_view("ABCDEFGHIJ")) {
            for (int i : {1, 2, 3}) v.push_back(std::string("P") + c + std::to_string(i));
        }
        v.emplace_back("PK1");
        for (const char c : std::string_view("LMNOPQRSUVWXYZ")) {
            for (int i : {1, 2}) v.push_back(std::string("P") + c + std::to_string(i));
        }
        v.emplace_back("PAA1");
        v.emplace_back("PAA2");
        return v;
    }();
    return names;
}

Blocks extract(std::string_view fortran) {
    std::vector<double> flat;
    for (const std::string& nm : parm7_names()) {
        const std::vector<double> v = read_data(fortran, nm);
        if (v.size() != 50) throw Fatal(nm + ": expected 50 values, parsed " + std::to_string(v.size()));
        flat.insert(flat.end(), v.begin(), v.end());
    }
    if (flat.size() != kParm7Total) throw Fatal("COMMON/PARM7/ came to " + std::to_string(flat.size()) + ", not 3200");

    Blocks blocks;
    std::size_t off = 0;
    for (const View& view : kViews) {
        const std::size_t take = view.n * (view.m != 0 ? view.m : 1);
        Block b;
        b.values.assign(flat.begin() + static_cast<std::ptrdiff_t>(off), flat.begin() + static_cast<std::ptrdiff_t>(off + take));
        b.n = view.n;
        b.m = view.m;
        blocks.emplace_back(view.name, std::move(b));
        off += take;
    }
    for (const View& flat_view : kFlats) {
        std::vector<double> v = read_data(fortran, flat_view.name);
        const std::size_t take = flat_view.n * (flat_view.m != 0 ? flat_view.m : 1);
        if (v.size() != take) throw Fatal(std::string(flat_view.name) + ": expected " + std::to_string(take) + ", parsed " + std::to_string(v.size()));
        Block b;
        b.values = std::move(v);
        b.n = flat_view.n;
        b.m = flat_view.m;
        std::string lower(flat_view.name);
        for (char& c : lower) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        blocks.emplace_back(std::move(lower), std::move(b));
    }
    return blocks;
}

std::string format_value(double v) {
    std::string s = dk::py_format_e(v, 6);
    if (s.empty() || s.front() != '-') s.insert(s.begin(), ' ');
    return s;
}

std::string format_values(const std::vector<double>& values, std::size_t indent) {
    const std::string pad(indent, ' ');
    std::string out;
    std::string row;
    std::size_t in_row = 0;
    const auto flush = [&] {
        if (!out.empty()) out += '\n';
        out += pad + row + ",";
        row.clear();
        in_row = 0;
    };
    for (const double v : values) {
        if (in_row != 0) row += ", ";
        row += format_value(v);
        if (++in_row == 5) flush();
    }
    if (in_row != 0) flush();
    return out;
}

std::string emit(std::string_view sha256_of_source, const Blocks& blocks, std::size_t ndata) {
    std::vector<std::string> L;
    const auto w = [&L](std::string line) { L.push_back(std::move(line)); };
    w("#pragma once");
    w("// msis_coefficients.hpp — GENERATED.  Do not edit.");
    w("//");
    w("// Produced by tools/msis_coefficients.cpp from the hash-pinned NRL reference");
    w("// implementation NRLMSISE-00.FOR, sha256 " + std::string(sha256_of_source));
    w("//");
    w("// NRLMSISE-00 has no published closed form: the model IS these coefficients");
    w("// together with the code that combines them, which is why plan §4 rule 6 calls");
    w("// the reference normative rather than an oracle.  They are EXTRACTED rather than");
    w("// transcribed because there are 3 300 of them.");
    w("//");
    w("// BLOCK DATA GTD7BK fills COMMON/PARM7/ through " + std::to_string(ndata) + " fifty-element DATA arrays");
    w("// PT1..PAA2.  Every subroutine reads that same storage through DIFFERENT names —");
    w("// pt(150), pd(150,9), ps(150), pdl(25,2), ptl(100,4), pma(100,10), sam(100),");
    w("// which is 3200 again.  The views below are that aliasing made explicit; the");
    w("// two-dimensional ones are COLUMN-MAJOR as FORTRAN stores them, so pd[j][i] here");
    w("// is PD(i+1, j+1) there.");
    w("");
    w("#include <array>");
    w("");
    w("namespace odl::atmosphere::coeff {");
    w("");
    for (const auto& [name, block] : blocks) {
        std::string upper = name;
        for (char& c : upper) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        if (block.m == 0) {
            w("inline constexpr std::array<double, " + std::to_string(block.n) + "> k" + upper + " = {");
            w(format_values(block.values));
            w("};");
            w("");
        } else {
            w("inline constexpr std::array<std::array<double, " + std::to_string(block.n) + ">, " + std::to_string(block.m) + "> k" + upper + " = {{");
            for (std::size_t j = 0; j < block.m; ++j) {
                const std::vector<double> col(block.values.begin() + static_cast<std::ptrdiff_t>(j * block.n),
                                              block.values.begin() + static_cast<std::ptrdiff_t>((j + 1) * block.n));
                w("    {");
                w(format_values(col, 8));
                w("    },");
            }
            w("}};");
            w("");
        }
    }
    w("}  // namespace odl::atmosphere::coeff");
    std::string text;
    for (const std::string& line : L) {
        text += line;
        text += '\n';
    }
    return text;
}

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() {
    Settings settings;
    settings.root = default_root();
    return settings;
}

namespace {

const char kUsageText[] = "usage: msis_coefficients [-h] [--root ROOT] [--out OUT] [--check]\n";

const char kHelpText[] =
    "\n"
    "Extract NRLMSISE-00's fitted coefficients from the pinned FORTRAN (manifest entry nrlmsise00-fortran) as generated source, modules/atmosphere/src/msis_coefficients.hpp: 3,300 numbers, read\n"
    "through the aliasing of COMMON/PARM7/ (SPEC-atmosphere ATMO-R-001/002).  Not part of the build; --check is a ctest.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree to read the manifest and the cache of, and to write into (default: the tree this tool was built from)\n"
    "  --out OUT    the generated header, relative to the tree unless absolute (default: modules/atmosphere/src/msis_coefficients.hpp)\n"
    "  --check      do not write: exit 1 unless the committed header is exactly what the extraction emits\n"
    "\n"
    "exit codes: 0 written / the committed header reproduces   1 it differs, or the FORTRAN is not the shape the extraction expects   2 an argument error, or a file that is missing or cannot be read\n"
    "            70 an error the tool did not anticipate\n";

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        std::filesystem::path root = settings.root;
        std::string out_given = kDefaultOut;
        bool check = false;
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
            if (opt == "--root" || opt == "--out") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument " + opt + ": expected one argument");
                    value = argv[++i];
                }
                if (opt == "--root") {
                    root = value;
                } else {
                    out_given = value;
                }
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        try {
            // doc = json.loads((ROOT / "manifest" / "manifest.json").read_text())
            const std::filesystem::path manifest_path = root / "manifest" / "manifest.json";
            dk::Json doc;
            try {
                doc = dk::Json::parse(dk::read_text(manifest_path));
            } catch (const dk::JsonError& exc) {
                throw BadInput(manifest_path.string() + ": " + exc.reason + ": line " + std::to_string(exc.line) + " column " + std::to_string(exc.column));
            }
            const dk::Json* entries = doc.find("entries");
            const dk::Json* cache = doc.find("cache");
            if (entries == nullptr || !entries->is_array() || cache == nullptr || !cache->is_string()) throw BadInput(manifest_path.string() + ": no \"entries\" list or no \"cache\" string");
            const dk::Json* entry = nullptr;
            for (const dk::Json& e : entries->as_array()) {
                const dk::Json* id = e.is_object() ? e.find("id") : nullptr;
                if (id == nullptr || !id->is_string()) throw BadInput(manifest_path.string() + ": an entry without an id");
                if (id->as_string() == kSourceId) {
                    entry = &e;
                    break;
                }
            }
            if (entry == nullptr) throw BadInput(manifest_path.string() + ": no entry " + kSourceId);
            const dk::Json* filename = entry->find("filename");
            const dk::Json* pinned = entry->find("sha256");
            if (filename == nullptr || !filename->is_string() || pinned == nullptr || !pinned->is_string()) throw BadInput(std::string(kSourceId) + ": no \"filename\" or no \"sha256\" in the manifest");
            const std::filesystem::path src = root / cache->as_string() / kSourceId / filename->as_string();

            const std::string sha = dk::sha256_hex(dk::read_bytes(src));
            if (sha != pinned->as_string()) throw Fatal(std::string(kSourceId) + ": cached bytes are not the pinned bytes");
            const std::string text = dk::read_text_lossy(src);

            const Blocks blocks = extract(text);
            const std::string text_out = emit(sha, blocks, parm7_names().size());
            const std::filesystem::path out = root / out_given;
            if (check) {
                bool same = false;
                if (std::filesystem::exists(out)) same = dk::universal_newlines(dk::read_text(out)) == text_out;
                if (!same) {
                    io.err << "REGENERATED COEFFICIENTS DIFFER from the committed file\n";
                    return kFailed;
                }
                io.out << "ok  " << out_given << " reproduces from " << kSourceId << '\n';
                return kOk;
            }
            dk::write_text(out, text_out);
            std::size_t total = 0;
            for (const auto& entry_block : blocks) total += entry_block.second.values.size();
            io.out << "wrote " << out_given << ": " << total << " coefficients (" << parm7_names().size() << " DATA arrays -> 3200 flat -> " << std::size(kViews) << " views, plus " << std::size(kFlats)
                   << " tables)\n";
            return kOk;
        } catch (const Fatal& exc) {
            io.err << exc.what() << '\n';
            return kFailed;
        } catch (const BadInput& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        } catch (const std::runtime_error& exc) {   // dk::read_bytes, read_text, write_text: the path and the reason
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::msis_coefficients

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::msis_coefficients::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
