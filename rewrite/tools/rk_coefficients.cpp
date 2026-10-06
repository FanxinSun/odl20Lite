// rk_coefficients.cpp — RKF7(8)'s Butcher tableau, transcribed and then PROVED.
//
// Plan L0 step 8, group C6, ported to C++ (the user's directive of 2026-10-06) from tools/rk_coefficients.py, on the devkit's BigInt and Rational.
//
// SPEC-integrators.  Plan §4 rule 6's converse: where an independent specification exists -- and it need not be a document -- the artifact's legibility stops mattering.  Ask what the thing must satisfy
// before asking how cleanly it prints.
//
// THE SOURCE PRINTS EVERYTHING AND OCR DESTROYS ALL OF IT.  Fehlberg, NASA TR R-287 (1968), Table X, is a scan.  `pdftotext` renders its beta block as
//
//     83_ = 841 = B_I = 8sl = B71 = Be1 = B_l = 81ol = _m = _12_ = 0
//
// and mangles even the contents page -- "TABLE IN." for III, "8O" for 80, the I/1 and O/0 confusions that matter most for digits.  THE PAGE IMAGES ARE PERFECTLY LEGIBLE at 300 dpi, and the coefficients there
// are EXACT RATIONALS.
//
// So the numbers below were read by eye from the page image.  That would be unacceptable on its own, and it does not have to be acceptable on its own, because a Runge-Kutta tableau has an independent
// specification: THE ORDER CONDITIONS, which are exact algebraic identities over the rationals.  A misread digit fails them.  The question is not whether a 1968 scan can be read reliably; it is whether what
// was read is checkable, and it is, completely.
//
// WHAT THIS TOOL PROVES, in exact rational arithmetic and never in floating point:
//
//   * row-sum consistency, sum_j beta_ij = alpha_i, on all 13 rows;
//   * c satisfies all 85 order conditions through order 7;
//   * chat satisfies all 200 through order 8;
//   * c VIOLATES exactly 40 of the 115 order-8 conditions -- which is what makes the pair an estimator rather than two names for one method.
//
// That last number is an INDEPENDENT CROSS-CHECK on the transcription that does not touch the coefficient digits at all.  There are 115 rooted trees of order 8, which is exactly the range of Fehlberg's error
// coefficients T_v (v = 1...115), and his Section XV says in prose: "our formula RK7(8) contains only 40 non-zero error coefficients T_v".  Forty is what this tool counts.  Prose in one part of the report and
// mathematics applied to a table in another agree, and neither is the scan's rendering of the digits.
//
//   rk_coefficients [--check] [--out FILE] [--root DIR]
//   exit 0 the proof holds and the header was written (or equals the committed one)   1 the proof fails, or the committed header differs   2 an argument error, or a file that cannot be read or written
//   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's product is a FILE, the generated header, whose committed copy is the same at every one of the ten commits whose ci runs are kept (gate 10 was green at each: the Python's
// regenerated text EQUALED it).  The port, run on an export of each of those ten commits, wrote that file byte for byte, against a substitution list registered before the comparison and of ONE entry (the
// generator's own name in line 4 of the header, `tools/rk_coefficients.py` -> `.cpp`); and with the header's line 4 so changed, `--check` printed the recorded gate-10 line.  What no committed record holds --
// the five verification lines of the non-check mode, the texts of a failed proof -- stands on lines derived from the Python's print statements, on tests that damage a copy of the tableau, and on rule 5.
// The rational arithmetic stands on tests of its own (tools/devkit/tests/bigint_tests.cpp, rational_tests.cpp); the rooted trees on the sequence 1, 1, 2, 4, 9, 20, 48, 115 (A000081), on the densities of
// the trees of orders 3 to 5, and on the order conditions of Euler's method, the midpoint method and the classical RK4, which are known in closed form.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.  `--out` is relative to it, an absolute path being used as it is, as
//     pathlib's `ROOT / out` did.
//   * The Python's argparse took any unambiguous abbreviation of an option (`--ch`, `--ou`); here the names are exact.  `-h` prints this tool's own text.  An option's value is whatever follows it
//     unless that begins with `--` (argparse refuses one that begins with a single dash too, so a file called -x is a value here), and `-h=x` and `--check=x` are refused.
//   * A committed header that exists and cannot be read (a directory, a link to one, a file that is not UTF-8) is REFUSED, exit 2, naming it: the Python died with a traceback, whose exit status 1 is the
//     one it also used for "the header differs".  A header that cannot be WRITTEN is refused the same way.  Text read back is compared as Python's read_text() hands it back: universal newlines.
//   * The order conditions are evaluated on SHARED sums: the weights of a tree are the product, over its subtrees, of sum_j beta_ij * (the weights of the subtree)_j, and that sum depends on the subtree and the
//     stage alone, so it is made once per subtree where the Python made it once per occurrence.  The arithmetic is exact, so the numbers are the same; only the number of operations is not.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "rk_coefficients.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>

namespace odl::tools::rk_coefficients {

namespace dk = odl::devkit;
namespace fs = std::filesystem;

namespace {

constexpr const char* kTool = "rk_coefficients";
constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;
constexpr const char* kDefaultOut = "modules/integrators/include/odl/integrators/rkf78_coefficients.hpp";
constexpr unsigned kPropagatingOrder = 7;   // c propagates at order 7 ...
constexpr unsigned kEstimatorOrder = 8;     // ... and the companion chat is of order 8

using Io = dk::Streams;

Rational R(std::int64_t numerator, std::int64_t denominator = 1) { return Rational(dk::BigInt(numerator), dk::BigInt(denominator)); }

std::string num(std::size_t n) { return std::to_string(n); }

// float(x), printed as f"{x:.17g}": the shortest text of 17 significant digits that round-trips, with the trailing zeros dropped
std::string g17(const Rational& x) {
    char buffer[40];
    std::snprintf(buffer, sizeof buffer, "%.17g", x.to_double());
    return buffer;
}

std::string doubles(const std::vector<Rational>& values) {
    std::string out;
    for (std::size_t i = 0; i < values.size(); ++i) out += (i != 0 ? ", " : "") + g17(values[i]);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// the rooted trees
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

// A rooted tree, interned by its canonical form: the repr of the nested tuple of its subtrees, themselves sorted by repr (the Python's `tuple(sorted(..., key=repr))`).  A leaf is `()`, a vertex with
// one leaf `((),)`, with two `((), ())`.  Isomorphic trees have one canonical form, which is how each tree is counted once.
struct Tree {
    std::string repr;
    unsigned order = 0;
    std::vector<std::size_t> kids;   // ids of the subtrees, in the order of their reprs
};

class Trees {
public:
    explicit Trees(unsigned p) : by_order_(p + 1) {
        for (unsigned n = 1; n <= p; ++n) build(n);
    }
    [[nodiscard]] const std::vector<Tree>& nodes() const { return nodes_; }
    [[nodiscard]] const std::vector<std::size_t>& of_order(unsigned n) const { return by_order_[n]; }

private:
    std::vector<Tree> nodes_;
    std::vector<std::vector<std::size_t>> by_order_;   // ids of each order, in the order of the reprs

    static std::string repr_of(const std::vector<std::string>& kids) {
        std::string out = "(";
        for (std::size_t i = 0; i < kids.size(); ++i) out += (i != 0 ? ", " : "") + kids[i];
        if (kids.size() == 1) out += ",";
        return out + ")";
    }

    // every multiset of subtrees whose orders add up to `total`, taken with non-increasing orders (each order at most `mx`), as the Python's parts() yields them
    void parts(unsigned total, unsigned mx, std::vector<std::size_t>& chosen, std::map<std::string, std::vector<std::size_t>>& found) const {
        if (total == 0) {
            std::vector<std::size_t> kids = chosen;
            std::sort(kids.begin(), kids.end(), [this](std::size_t a, std::size_t b) { return nodes_[a].repr < nodes_[b].repr; });
            std::vector<std::string> reprs;
            for (const std::size_t k : kids) reprs.push_back(nodes_[k].repr);
            found.emplace(repr_of(reprs), kids);
            return;
        }
        for (unsigned k = std::min(total, mx); k >= 1; --k) {
            for (const std::size_t t : by_order_[k]) {
                chosen.push_back(t);
                parts(total - k, k, chosen, found);
                chosen.pop_back();
            }
        }
    }

    void build(unsigned n) {
        std::map<std::string, std::vector<std::size_t>> found;   // repr -> its subtrees; sorted by repr, as the Python's sorted(out, key=repr)
        std::vector<std::size_t> chosen;
        parts(n - 1, n - 1, chosen, found);   // for n == 1 the one multiset is the empty one: the single vertex, `()`
        for (const auto& [repr, kids] : found) {
            by_order_[n].push_back(nodes_.size());
            nodes_.push_back(Tree{repr, n, kids});
        }
    }
};

}  // namespace

std::vector<std::size_t> tree_counts(unsigned p) {
    const Trees trees(p);
    std::vector<std::size_t> counts;
    for (unsigned n = 1; n <= p; ++n) counts.push_back(trees.of_order(n).size());
    return counts;
}

Rational Tableau::beta_at(std::size_t i, std::size_t j) const { return j < beta[i].size() ? beta[i][j] : Rational(0); }

void Tableau::check_shape() const {
    if (beta.size() != alpha.size() || c.size() != alpha.size() || chat.size() != alpha.size()) {
        throw std::invalid_argument("a tableau with " + std::to_string(alpha.size()) + " nodes, " + std::to_string(beta.size()) + " rows of coefficients, " + std::to_string(c.size()) + " weights and " +
                                    std::to_string(chat.size()) + " companion weights: the four must agree");
    }
}

const Tableau& fehlberg_table_x() {
    // TABLE X, RK 7(8), report page 65 (PDF page 72), read at 300 dpi.
    static const Tableau table = {
        {R(0), R(2, 27), R(1, 9), R(1, 6), R(5, 12), R(1, 2), R(5, 6), R(1, 6), R(2, 3), R(1, 3), R(1), R(0), R(1)},
        {
            {},
            {R(2, 27)},
            {R(1, 36), R(1, 12)},
            {R(1, 24), R(0), R(1, 8)},
            {R(5, 12), R(0), R(-25, 16), R(25, 16)},
            {R(1, 20), R(0), R(0), R(1, 4), R(1, 5)},
            {R(-25, 108), R(0), R(0), R(125, 108), R(-65, 27), R(125, 54)},
            {R(31, 300), R(0), R(0), R(0), R(61, 225), R(-2, 9), R(13, 900)},
            {R(2), R(0), R(0), R(-53, 6), R(704, 45), R(-107, 9), R(67, 90), R(3)},
            {R(-91, 108), R(0), R(0), R(23, 108), R(-976, 135), R(311, 54), R(-19, 60), R(17, 6), R(-1, 12)},
            {R(2383, 4100), R(0), R(0), R(-341, 164), R(4496, 1025), R(-301, 82), R(2133, 4100), R(45, 82), R(45, 164), R(18, 41)},
            {R(3, 205), R(0), R(0), R(0), R(0), R(-6, 41), R(-3, 205), R(-3, 41), R(3, 41), R(6, 41), R(0)},
            {R(-1777, 4100), R(0), R(0), R(-341, 164), R(4496, 1025), R(-289, 82), R(2193, 4100), R(51, 82), R(33, 164), R(12, 41), R(0), R(1)},
        },
        {R(41, 840), R(0), R(0), R(0), R(0), R(34, 105), R(9, 35), R(9, 35), R(9, 280), R(9, 280), R(41, 840), R(0), R(0)},
        {R(0), R(0), R(0), R(0), R(0), R(34, 105), R(9, 35), R(9, 35), R(9, 280), R(9, 280), R(0), R(41, 840), R(41, 840)},
    };
    return table;
}

std::vector<OrderCondition> order_conditions(const Tableau& tableau, unsigned p) {
    tableau.check_shape();
    const std::size_t stages = tableau.stages();
    const Trees trees(p);
    const std::vector<Tree>& nodes = trees.nodes();
    std::vector<Rational> density(nodes.size());
    std::vector<std::vector<Rational>> phi(nodes.size());
    // sums[t][i] = sum_{j<i} beta(i, j) * phi(t)[j]: what stage i contributes to a tree that has t as a subtree.  The nodes are in order of their order, so a tree's subtrees come before it.
    std::vector<std::vector<Rational>> sums(nodes.size());
    for (std::size_t id = 0; id < nodes.size(); ++id) {
        const Tree& t = nodes[id];
        Rational gamma(static_cast<std::int64_t>(t.order));
        for (const std::size_t kid : t.kids) gamma *= density[kid];
        density[id] = gamma;
        if (t.kids.empty()) {
            phi[id].assign(stages, Rational(1));
        } else {
            phi[id].reserve(stages);
            for (std::size_t i = 0; i < stages; ++i) {
                Rational product(1);
                for (const std::size_t kid : t.kids) product *= sums[kid][i];
                phi[id].push_back(product);
            }
        }
        sums[id].reserve(stages);
        for (std::size_t i = 0; i < stages; ++i) {
            Rational total(0);
            for (std::size_t j = 0; j < i; ++j) {
                const Rational b = tableau.beta_at(i, j);
                if (!b.is_zero()) total += b * phi[id][j];
            }
            sums[id].push_back(total);
        }
    }
    std::vector<OrderCondition> out;
    for (unsigned n = 1; n <= p; ++n) {
        for (const std::size_t id : trees.of_order(n)) out.push_back(OrderCondition{n, nodes[id].repr, density[id], phi[id]});
    }
    return out;
}

namespace {

// the conditions of `all` (of any order up to the largest in it) that `weights` does not satisfy, through order p
std::vector<OrderCondition> violated(const std::vector<OrderCondition>& all, const std::vector<Rational>& weights, unsigned p) {
    std::vector<OrderCondition> bad;
    for (const OrderCondition& condition : all) {
        if (condition.order > p) continue;
        Rational lhs(0);
        for (std::size_t i = 0; i < weights.size(); ++i) lhs += weights[i] * condition.phi[i];
        if (lhs != Rational(1) / condition.density) bad.push_back(condition);
    }
    return bad;
}

}  // namespace

std::vector<OrderCondition> violations(const Tableau& tableau, const std::vector<Rational>& weights, unsigned p) {
    if (weights.size() != tableau.stages()) throw std::invalid_argument(std::to_string(weights.size()) + " weights for a tableau of " + std::to_string(tableau.stages()) + " stages");
    return violated(order_conditions(tableau, p), weights, p);
}

Stats verify(const Tableau& tableau, std::size_t fehlberg_nonzero_t) {
    tableau.check_shape();
    const std::size_t stages = tableau.stages();
    std::size_t rows_ok = 0;
    for (std::size_t i = 0; i < stages; ++i) {
        Rational total(0);
        for (const Rational& b : tableau.beta[i]) total += b;
        if (total == tableau.alpha[i]) ++rows_ok;
    }
    if (rows_ok != stages) throw VerificationFailure("row-sum consistency fails: the transcription is wrong");
    const std::vector<OrderCondition> all = order_conditions(tableau, kEstimatorOrder);
    const std::vector<OrderCondition> v7 = violated(all, tableau.c, kPropagatingOrder);
    const std::vector<OrderCondition> v8 = violated(all, tableau.chat, kEstimatorOrder);
    std::size_t c_order8 = 0;
    for (const OrderCondition& condition : violated(all, tableau.c, kEstimatorOrder)) {
        if (condition.order == kEstimatorOrder) ++c_order8;
    }
    Stats stats;
    stats.rows = rows_ok;
    for (const OrderCondition& condition : all) {
        if (condition.order <= kPropagatingOrder) ++stats.c_trees;
        ++stats.chat_trees;
        if (condition.order == kEstimatorOrder) ++stats.order8_trees;
    }
    if (!v7.empty()) throw VerificationFailure("c violates " + num(v7.size()) + " order conditions through order 7");
    if (!v8.empty()) throw VerificationFailure("chat violates " + num(v8.size()) + " order conditions through order 8");
    if (c_order8 != fehlberg_nonzero_t) {
        throw VerificationFailure("c violates " + num(c_order8) + " order-8 conditions; Fehlberg's Section XV prints " + num(fehlberg_nonzero_t) + " non-zero error coefficients. These must agree.");
    }
    stats.c_order8_violations = c_order8;
    return stats;
}

std::string emit(const Tableau& tableau, const Stats& stats) {
    std::string text;
    const auto line = [&text](const std::string& s) {
        text += s;
        text += '\n';
    };
    const std::size_t stages = tableau.stages();
    line("#pragma once");
    line("// rkf78_coefficients.hpp — GENERATED.  Do not edit.");
    line("//");
    line("// Produced by tools/rk_coefficients.cpp from Fehlberg, NASA TR R-287 (1968),");
    line(std::string("// Table X (report p.65), sha256 ") + kSourceSha256);
    line("//");
    line("// READ FROM THE PAGE IMAGE, NOT THE TEXT LAYER.  The report is a 1968 scan and");
    line("// pdftotext returns noise for its mathematics; the images at 300 dpi are exact");
    line("// and give the coefficients as RATIONALS.  See the manifest entry's verify_note.");
    line("//");
    line("// AND THE TRANSCRIPTION IS PROVED, NOT TRUSTED.  In exact rational arithmetic:");
    line("//   row-sum consistency   " + num(stats.rows) + "/" + num(stats.rows) + " rows, sum_j beta_ij = alpha_i");
    line("//   c    through order 7  " + num(stats.c_trees) + " order conditions, 0 violated");
    line("//   chat through order 8  " + num(stats.chat_trees) + " order conditions, 0 violated");
    line("//   c    at order 8       " + num(stats.c_order8_violations) + " of " + num(stats.order8_trees) + " violated");
    line("//");
    line("// That last line is an INDEPENDENT CROSS-CHECK that never touches the digits:");
    line("// there are " + num(stats.order8_trees) + " rooted trees of order 8, exactly the range of");
    line("// Fehlberg's error coefficients T_v, and his Section XV says in PROSE that RK7(8)");
    line("// has \"only " + num(kFehlbergNonzeroT) + " non-zero error coefficients T_v\".  Prose on one page and");
    line("// mathematics applied to a table on another agree.");
    line("//");
    line("// THE ESTIMATOR IS IDENTICALLY ZERO ON A QUADRATURE PROBLEM.  alpha_0 = alpha_11 = 0");
    line("// and alpha_10 = alpha_12 = 1, and the truncation term (134) is");
    line("//     TE = (41/840)(f0 + f10 - f11 - f12) h,");
    line("// so for any right-hand side depending on x alone the four evaluations cancel in");
    line("// pairs and the estimate is not small but ZERO.  See SPEC-integrators; the");
    line("// controller is blind there, not merely optimistic.");
    line("");
    line("#include <array>");
    line("");
    line("namespace odl::integrators::rkf78 {");
    line("");
    line("inline constexpr int kStages = " + num(stages) + ";");
    line("");
    line("inline constexpr std::array<double, kStages> kAlpha = {");
    line("    " + doubles(tableau.alpha));
    line("};");
    line("");
    line("/// Row-major, lower triangular; entries above the diagonal are zero.");
    line("inline constexpr std::array<std::array<double, kStages>, kStages> kBeta = {{");
    for (std::size_t i = 0; i < stages; ++i) {
        std::vector<Rational> row;
        for (std::size_t j = 0; j < stages; ++j) row.push_back(tableau.beta_at(i, j));
        line("    {" + doubles(row) + "},");
    }
    line("}};");
    line("");
    line("/// The propagating 7th-order weights.");
    line("inline constexpr std::array<double, kStages> kC = {");
    line("    " + doubles(tableau.c));
    line("};");
    line("");
    line("/// The 8th-order companion, used only for the error estimate.");
    line("inline constexpr std::array<double, kStages> kCHat = {");
    line("    " + doubles(tableau.chat));
    line("};");
    line("");
    line("/// (134): TE = kErrorWeight * (f0 + f10 - f11 - f12) * h");
    line("inline constexpr double kErrorWeight = " + g17(Rational(41, 840)) + ";");
    line("inline constexpr int kOrder = 7;          ///< the order that PROPAGATES");
    line("inline constexpr int kEstimatorOrder = 8;");
    line("inline constexpr int kOrder8Violations = " + num(stats.c_order8_violations) + ";");
    line("");
    line("}  // namespace odl::integrators::rkf78");
    return text;
}

namespace {

const char kUsageText[] = "usage: rk_coefficients [-h] [--out OUT] [--check] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "RKF7(8)'s Butcher tableau, proved in exact rational arithmetic (SPEC-integrators INTG-A-001): every row's coefficients sum to its node, c satisfies all 85 order conditions through order 7,\n"
    "chat all 200 through order 8, and c violates exactly the 40 of the 115 order-8 conditions that Fehlberg's Section XV counts; then the header the proof licenses is written, or with --check compared\n"
    "with the committed one.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --out OUT    the header, relative to the tree (default: modules/integrators/include/odl/integrators/rkf78_coefficients.hpp)\n"
    "  --check      verify, and compare the regenerated header with the committed one instead of writing it\n"
    "  --root ROOT  the tree (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 the proof holds and the header was written or equals the committed one   1 the proof fails, or the committed header differs\n"
    "            2 an argument error, or a file that cannot be read or written\n";

}  // namespace

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

int run_on(const Tableau& table, const std::vector<std::string>& argv, Io io) {
    try {
        std::string root_given = default_root().string();
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
                (opt == "--root" ? root_given : out_given) = value;
            } else if (opt == "--check" && !has_value) {
                check = true;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        Stats stats;
        try {
            stats = verify(table);
        } catch (const VerificationFailure& failure) {
            io.err << failure.what() << '\n';
            return kFailed;
        }
        if (!check) {
            io.out << "  row-sum consistency          " << stats.rows << "/" << table.stages() << " rows, exactly\n";
            io.out << "  c    through order 7         " << stats.c_trees << " trees, 0 violated\n";
            io.out << "  chat through order 8         " << stats.chat_trees << " trees, 0 violated\n";
            io.out << "  c    at order 8              " << stats.c_order8_violations << " of " << stats.order8_trees << " violated\n";
            io.out << "  Fehlberg's printed count     " << kFehlbergNonzeroT << " non-zero T_v  -> AGREES\n";
        }
        const std::string text = emit(table, stats);
        const fs::path out = fs::path(root_given) / out_given;
        if (check) {
            std::error_code ec;
            bool same = false;
            if (fs::exists(out, ec)) {
                try {
                    same = dk::universal_newlines(dk::read_text(out)) == text;
                } catch (const std::runtime_error& exc) {
                    io.err << kTool << ": " << exc.what() << '\n';
                    return kArgument;
                }
            }
            if (!same) {
                io.err << "REGENERATED COEFFICIENTS DIFFER from the committed file\n";
                return kFailed;
            }
            io.out << "ok  " << out_given << " reproduces, and its order conditions hold exactly\n";
            return kOk;
        }
        try {
            dk::write_text(out, text);
        } catch (const std::runtime_error& exc) {
            io.err << kTool << ": " << exc.what() << '\n';
            return kArgument;
        }
        io.out << "wrote " << out_given << '\n';
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Io io) { return run_on(fehlberg_table_x(), argv, io); }

}  // namespace odl::tools::rk_coefficients

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::rk_coefficients::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
