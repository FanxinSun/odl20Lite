#pragma once
// tools/rk_coefficients.hpp — RKF7(8)'s Butcher tableau, transcribed and then PROVED: the tool's entry point and the pieces its tests call (plan L0 step 8, group C6).
//
// `run` is the whole tool: `rk_coefficients [--check] [--out FILE] [--root DIR]`; it proves the tableau in exact rational arithmetic and writes (or, with --check, compares) the header
// modules/integrators/include/odl/integrators/rkf78_coefficients.hpp.  Exit 0 the proof holds and the header was written or equals what is committed, 1 the proof fails or the committed header differs, 2 an
// argument error or a file that cannot be read or written, 70 an error the tool did not anticipate.  ci.sh's gate 10 runs the built tool with --check.
//
// The pieces: the tableau as data (Rational, so that nothing is ever rounded until the header is written), the rooted trees with their densities and elementary weights, the order conditions each tableau
// and weight vector must satisfy, the verification, and the text of the header.  They take the tableau as an argument so that a test can hand them a damaged one.

#include <odl/devkit/rational.hpp>
#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace odl::tools::rk_coefficients {

using odl::devkit::Rational;

/// A Butcher tableau with the two weight vectors of an embedded pair: `alpha` are the nodes, row i of `beta` holds the i coefficients below the diagonal of stage i (a stage's coefficients that are not
/// listed are zero), `c` the weights that propagate and `chat` those of the companion that estimates the error.  The four have one entry per stage (beta: one row per stage).
struct Tableau {
    std::vector<Rational> alpha;
    std::vector<std::vector<Rational>> beta;
    std::vector<Rational> c;
    std::vector<Rational> chat;

    [[nodiscard]] std::size_t stages() const { return alpha.size(); }
    /// beta(i, j) of the Python: the coefficient of stage j in stage i, zero where the row does not reach.
    [[nodiscard]] Rational beta_at(std::size_t i, std::size_t j) const;
    /// Throws std::invalid_argument when the four do not agree on the number of stages.
    void check_shape() const;
};

/// Fehlberg, NASA TR R-287 (1968), Table X, RK 7(8), report page 65 (PDF page 72), read at 300 dpi from the page image.
[[nodiscard]] const Tableau& fehlberg_table_x();

/// The SHA-256 of the report the table was read from (manifest entry fehlberg-tr-r-287), and the number of non-zero error coefficients T_v that Fehlberg's Section XV (p. 66) states for RK7(8).
inline constexpr const char* kSourceSha256 = "5553a2a3eb53785a461762cc2b29428015f1b32c3ad0a5cb57f85a421256a0c8";
inline constexpr std::size_t kFehlbergNonzeroT = 40;

/// One order condition: for the rooted tree `tree`, sum_i weights_i * weights_of_tree_i must equal 1 / density.
struct OrderCondition {
    unsigned order = 0;
    /// The tree in the Python's canonical form: the repr of the nested tuple of its subtrees, sorted by that repr.  `()` is the single vertex, `((),)` the vertex with one leaf, `((), ())` the vertex with two.
    std::string tree;
    Rational density;               // gamma(t) = order(t) * the product of the densities of the subtrees
    std::vector<Rational> phi;      // Phi(t): the elementary weight of each stage of the tableau
};

/// The number of rooted trees of each order, 1 to p, each tree once however its subtrees are ordered: A000081, 1, 1, 2, 4, 9, 20, 48, 115, ...  (independent of any tableau).
[[nodiscard]] std::vector<std::size_t> tree_counts(unsigned p);

/// The order conditions of the tableau through order p, one per rooted tree, ordered by order and then by the tree's canonical form.
[[nodiscard]] std::vector<OrderCondition> order_conditions(const Tableau& tableau, unsigned p);

/// The conditions through order p that `weights` does not satisfy.
[[nodiscard]] std::vector<OrderCondition> violations(const Tableau& tableau, const std::vector<Rational>& weights, unsigned p);

/// What the verification counted.
struct Stats {
    std::size_t rows = 0;                 // rows whose coefficients sum to their node
    std::size_t c_trees = 0;              // order conditions of c through order 7
    std::size_t chat_trees = 0;           // order conditions of chat through order 8
    std::size_t order8_trees = 0;         // rooted trees of order 8
    std::size_t c_order8_violations = 0;  // of those, the ones c violates
};

/// A proof that fails: the message is the Python's sys.exit text.
struct VerificationFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// Proves the tableau: every row's coefficients sum to its node, c satisfies all the order conditions through order 7, chat all through order 8, and c violates exactly Fehlberg's printed number of the order-8
/// ones.  Throws VerificationFailure, naming what failed, otherwise returns the counts.
[[nodiscard]] Stats verify(const Tableau& tableau, std::size_t fehlberg_nonzero_t = kFehlbergNonzeroT);

/// The text of the generated header: the numbers are the doubles nearest the rationals, printed with %.17g.
[[nodiscard]] std::string emit(const Tableau& tableau, const Stats& stats);

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` is the same on any tableau (the tests damage a copy and show every failure of the proof reported); `run` is `run_on` the real one.
[[nodiscard]] int run_on(const Tableau& tableau, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The tree this tool was built from (ODL_TREE_ROOT) or the current directory: what `--root` defaults to.
[[nodiscard]] std::filesystem::path default_root();

}  // namespace odl::tools::rk_coefficients
