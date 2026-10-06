// tests/devtools/rk_coefficients_tests.cpp — RKF7(8)'s tableau and the proof about it (plan L0 step 8, group C6): the rooted trees and their order conditions against what is known in closed form (the
// sequence 1, 1, 2, 4, 9, 20, 48, 115; the densities; Euler's method, the midpoint method and the classical RK4, whose conditions are worked out by hand below), the verification and every one of its
// failures on damaged copies of the table, the text of the generated header, the tool's command line and what it does to a file, and the real tree.  (ctests `rk_coefficients.behaviour` and
// `rk_coefficients.real_tree`; the Python tool had no test of its own, so these are the cases that show it CAN fail.)
//
// Every expectation is DERIVED BY HAND, from the order conditions' definition and from the print statements and f-strings of rk_coefficients.py, and not taken from running the port.

#include <catch2/catch_test_macros.hpp>

#include "bignum_printers.hpp"
#include "throwing_stream.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/json.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/tool.hpp>

#include "rk_coefficients.hpp"

#include <algorithm>
#include <filesystem>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace rk = odl::tools::rk_coefficients;
using namespace odl::devkit;

namespace {

constexpr int kOk = 0, kFailed = 1, kArgument = 2, kInternal = 70;
constexpr const char* kDefaultHeader = "modules/integrators/include/odl/integrators/rkf78_coefficients.hpp";

Rational q(std::int64_t n, std::int64_t d = 1) { return Rational(BigInt(n), BigInt(d)); }

bool contains(const std::string& haystack, const std::string& needle) { return haystack.find(needle) != std::string::npos; }

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

Result run_real(const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = rk::run(args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

Result run_on(const rk::Tableau& table, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = rk::run_on(table, args, Streams{out, err});
    return Result{code, out.str(), err.str()};
}

// the tableaux whose order conditions are known in closed form: explicit Euler (order 1), the midpoint method (order 2) and the classical Runge-Kutta method (order 4); the second weight vector is the first
rk::Tableau euler() { return {{q(0)}, {{}}, {q(1)}, {q(1)}}; }
rk::Tableau midpoint() { return {{q(0), q(1, 2)}, {{}, {q(1, 2)}}, {q(0), q(1)}, {q(0), q(1)}}; }
rk::Tableau classical() {
    return {{q(0), q(1, 2), q(1, 2), q(1)},
            {{}, {q(1, 2)}, {q(0), q(1, 2)}, {q(0), q(0), q(1)}},
            {q(1, 6), q(1, 3), q(1, 3), q(1, 6)},
            {q(1, 6), q(1, 3), q(1, 3), q(1, 6)}};
}

// the conditions of one order, from order_conditions(), by canonical form
std::vector<rk::OrderCondition> of_order(const std::vector<rk::OrderCondition>& all, unsigned order) {
    std::vector<rk::OrderCondition> out;
    for (const rk::OrderCondition& c : all) {
        if (c.order == order) out.push_back(c);
    }
    return out;
}

const rk::OrderCondition& by_tree(const std::vector<rk::OrderCondition>& all, const std::string& tree) {
    for (const rk::OrderCondition& c : all) {
        if (c.tree == tree) return c;
    }
    throw std::logic_error("no condition for the tree " + tree);
}

Rational weighted(const std::vector<Rational>& weights, const rk::OrderCondition& condition) {
    Rational total(0);
    for (std::size_t i = 0; i < weights.size(); ++i) total += weights[i] * condition.phi[i];
    return total;
}

// the five lines the verification prints, derived from the Python's print statements (the spacing is theirs)
const char kVerboseReal[] =
    "  row-sum consistency          13/13 rows, exactly\n"
    "  c    through order 7         85 trees, 0 violated\n"
    "  chat through order 8         200 trees, 0 violated\n"
    "  c    at order 8              40 of 115 violated\n"
    "  Fehlberg's printed count     40 non-zero T_v  -> AGREES\n";

const char kHelpFirstLine[] = "usage: rk_coefficients [-h] [--out OUT] [--check] [--root ROOT]\n";

}  // namespace

// ======================================================================================================================================== the rooted trees

TEST_CASE("there are 1, 1, 2, 4, 9, 20, 48 and 115 rooted trees of order 1 to 8, each counted once however its subtrees are ordered", "[rk_coefficients][behaviour]") {
    CHECK(rk::tree_counts(0).empty());
    CHECK(rk::tree_counts(1) == std::vector<std::size_t>{1});
    CHECK(rk::tree_counts(8) == (std::vector<std::size_t>{1, 1, 2, 4, 9, 20, 48, 115}));   // A000081
    // 85 conditions through order 7 and 200 through order 8, the numbers the proof is stated in
    const std::vector<std::size_t> counts = rk::tree_counts(8);
    std::size_t through7 = 0;
    std::size_t through8 = 0;
    for (std::size_t n = 0; n < counts.size(); ++n) {
        if (n < 7) through7 += counts[n];
        through8 += counts[n];
    }
    CHECK(through7 == 85);
    CHECK(through8 == 200);
}

TEST_CASE("the trees of orders 1 to 4 in their canonical forms, with the densities worked out by hand; those of order 5 by their densities", "[rk_coefficients][behaviour]") {
    const std::vector<rk::OrderCondition> all = rk::order_conditions(euler(), 5);
    // the canonical form is the repr of the nested tuple of subtrees, themselves in the order of their reprs: '(' sorts before ')' and ',', which puts a chain before a bush
    const auto forms = [&](unsigned order) {
        std::vector<std::pair<std::string, std::string>> out;   // tree, density
        for (const rk::OrderCondition& c : of_order(all, order)) out.emplace_back(c.tree, c.density.to_string());
        return out;
    };
    using Forms = std::vector<std::pair<std::string, std::string>>;
    CHECK(forms(1) == (Forms{{"()", "1"}}));                                                                       // gamma = 1
    CHECK(forms(2) == (Forms{{"((),)", "2"}}));                                                                    // 2 * gamma(leaf)
    CHECK(forms(3) == (Forms{{"(((),),)", "6"}, {"((), ())", "3"}}));                                              // chain: 3*2*1; bush: 3*1*1
    CHECK(forms(4) == (Forms{{"((((),),),)", "24"}, {"(((), ()),)", "12"}, {"(((),), ())", "8"}, {"((), (), ())", "4"}}));
    // order 5: the nine densities, and the canonical forms strictly increasing
    std::multiset<std::string> densities;
    std::string previous;
    for (const rk::OrderCondition& c : of_order(all, 5)) {
        densities.insert(c.density.to_string());
        CHECK(previous < c.tree);
        previous = c.tree;
    }
    CHECK(of_order(all, 5).size() == 9);
    CHECK(densities == (std::multiset<std::string>{"5", "10", "15", "20", "20", "30", "40", "60", "120"}));
    // the conditions come by order, and the elementary weights are one per stage
    unsigned last = 0;
    for (const rk::OrderCondition& c : all) {
        CHECK(c.order >= last);
        last = c.order;
        CHECK(c.phi.size() == 1);
    }
}

// ======================================================================================================================================== order conditions of known methods

TEST_CASE("the elementary weights of the midpoint method are the ones the definition gives: (0, 1/2) for the vertex with a leaf, (0, 0) for the chain of three, (0, 1/4) for the bush of three", "[rk_coefficients][behaviour]") {
    const std::vector<rk::OrderCondition> all = rk::order_conditions(midpoint(), 3);
    const auto vec = [](std::vector<Rational> v) { return v; };
    CHECK(by_tree(all, "()").phi == vec({q(1), q(1)}));
    CHECK(by_tree(all, "((),)").phi == vec({q(0), q(1, 2)}));       // the row sums: stage 1 is 1/2 of stage 0's weight 1
    CHECK(by_tree(all, "(((),),)").phi == vec({q(0), q(0)}));       // stage 1 sums beta(1,0) * (weight of stage 0 in the subtree) = 1/2 * 0
    CHECK(by_tree(all, "((), ())").phi == vec({q(0), q(1, 4)}));    // (1/2)^2
}

TEST_CASE("explicit Euler satisfies order 1 and violates 1 condition of order 2 and 2 of order 3; the midpoint method satisfies orders 1 and 2 and violates both of order 3", "[rk_coefficients][behaviour]") {
    const rk::Tableau e = euler();
    CHECK(rk::violations(e, e.c, 1).empty());
    const auto bad_e = rk::violations(e, e.c, 3);
    REQUIRE(bad_e.size() == 3);
    CHECK(bad_e[0].order == 2);
    CHECK(bad_e[1].order == 3);
    CHECK(bad_e[2].order == 3);
    CHECK(weighted(e.c, bad_e[0]) == q(0));   // sum b_i * 0: the first stage has nothing below it
    const rk::Tableau m = midpoint();
    CHECK(rk::violations(m, m.c, 2).empty());
    const auto bad_m = rk::violations(m, m.c, 3);
    REQUIRE(bad_m.size() == 2);
    CHECK(bad_m[0].order == 3);
    CHECK(bad_m[1].order == 3);
    // the two residuals: the chain gives sum b_i * (0, 0) = 0 against 1/6, the bush sum b_i * (0, 1/4) = 1/4 against 1/3
    CHECK(weighted(m.c, by_tree(rk::order_conditions(m, 3), "(((),),)")) == q(0));
    CHECK(weighted(m.c, by_tree(rk::order_conditions(m, 3), "((), ())")) == q(1, 4));
    CHECK(by_tree(rk::order_conditions(m, 3), "(((),),)").density == q(6));
    CHECK(by_tree(rk::order_conditions(m, 3), "((), ())").density == q(3));
}

TEST_CASE("the classical RK4 satisfies all 8 conditions through order 4 and violates all 9 of order 5, each by the amount nine formulas worked out by hand give", "[rk_coefficients][behaviour]") {
    const rk::Tableau t = classical();
    CHECK(rk::violations(t, t.c, 4).empty());
    CHECK(rk::order_conditions(t, 4).size() == 8);
    const auto bad = rk::violations(t, t.c, 5);
    CHECK(bad.size() == 9);
    // sum_i b_i * (elementary weight): c = (0, 1/2, 1/2, 1), Ac = (0, 0, 1/4, 1/2), Ac^2 = (0, 0, 1/8, 1/4), AAc = (0, 0, 0, 1/4), A(c.Ac) = AAc^2 = (0, 0, 0, 1/8), AAAc = 0, b = (1/6, 1/3, 1/3, 1/6):
    //   bush            sum b c^4        = 1/48 + 1/48 + 1/6 = 5/24            (density 5)
    //   c^2, [leaf]     sum b c^2 Ac     = 1/48 + 1/12       = 5/48            (10)
    //   [leaf], [leaf]  sum b Ac^2       = 1/48 + 1/24       = 1/16            (20)
    //   c, A c^2        sum b c Ac^2     = 1/48 + 1/24       = 1/16            (15)
    //   c, AAc          sum b c AAc      = 1/24                                (30)
    //   A c^3           sum b Ac^3       = 1/48 + 1/48       = 1/24            (20)
    //   A (c.Ac)        sum b A(c.Ac)    = 1/48                                (40)
    //   AA c^2          sum b AAc^2      = 1/48                                (60)
    //   AAA c           sum b AAAc       = 0                                   (120)
    std::multiset<std::string> got;
    for (const rk::OrderCondition& c : of_order(rk::order_conditions(t, 5), 5)) got.insert(c.density.to_string() + ":" + weighted(t.c, c).to_string());
    CHECK(got == (std::multiset<std::string>{"5:5/24", "10:5/48", "20:1/16", "15:1/16", "30:1/24", "20:1/24", "40:1/48", "60:1/48", "120:0"}));
    // and the four of order 4 hold exactly: 1/4, 1/8, 1/12, 1/24
    std::multiset<std::string> order4;
    for (const rk::OrderCondition& c : of_order(rk::order_conditions(t, 4), 4)) order4.insert(c.density.to_string() + ":" + weighted(t.c, c).to_string());
    CHECK(order4 == (std::multiset<std::string>{"24:1/24", "12:1/12", "8:1/8", "4:1/4"}));
}

TEST_CASE("an entry of beta on or above the diagonal is not read by the order conditions and is counted by the row sum, as in the Python", "[rk_coefficients][behaviour]") {
    rk::Tableau t = midpoint();
    t.beta[1].push_back(q(7, 9));   // beta(1, 1): on the diagonal of stage 1
    CHECK(t.beta_at(1, 1) == q(7, 9));
    // the elementary weight of the vertex with one leaf, at stage 1, is still beta(1, 0) * 1 = 1/2: the sum runs over the stages BELOW the diagonal
    CHECK(by_tree(rk::order_conditions(t, 2), "((),)").phi == (std::vector<Rational>{q(0), q(1, 2)}));
    // but the row no longer sums to its node, 1/2
    try {
        (void)rk::verify(t);
        FAIL("the proof of a tableau whose row does not sum to its node must fail");
    } catch (const rk::VerificationFailure& failure) {
        CHECK(std::string(failure.what()) == "row-sum consistency fails: the transcription is wrong");
    }
}

TEST_CASE("violations() refuses a weight vector or a tableau of the wrong shape", "[rk_coefficients][behaviour]") {
    const rk::Tableau t = midpoint();
    CHECK_THROWS_AS(rk::violations(t, {q(1)}, 2), std::invalid_argument);
    rk::Tableau bad = t;
    bad.c.pop_back();
    CHECK_THROWS_AS(rk::order_conditions(bad, 2), std::invalid_argument);
    CHECK_THROWS_AS(rk::verify(bad), std::invalid_argument);
    rk::Tableau no_row = t;
    no_row.beta.pop_back();
    CHECK_THROWS_AS(rk::order_conditions(no_row, 2), std::invalid_argument);
}

// ======================================================================================================================================== the proof, and its failures

TEST_CASE("the real table passes the proof: 13 rows, 85 conditions of c, 200 of chat, 40 of 115 at order 8 violated by c", "[rk_coefficients][behaviour]") {
    const rk::Tableau& real = rk::fehlberg_table_x();
    CHECK(real.stages() == 13);
    const rk::Stats stats = rk::verify(real);
    CHECK(stats.rows == 13);
    CHECK(stats.c_trees == 85);
    CHECK(stats.chat_trees == 200);
    CHECK(stats.order8_trees == 115);
    CHECK(stats.c_order8_violations == 40);
    CHECK(rk::kFehlbergNonzeroT == 40);
    // the table is lower triangular as transcribed: row i lists i entries
    for (std::size_t i = 0; i < real.stages(); ++i) CHECK(real.beta[i].size() == i);
    CHECK(real.beta_at(2, 1) == q(1, 12));
    CHECK(real.beta_at(2, 2) == q(0));      // on the diagonal and beyond: zero
    CHECK(real.beta_at(2, 12) == q(0));
    CHECK(real.alpha[1] == q(2, 27));
    CHECK(real.c[0] == q(41, 840));
    CHECK(real.chat[12] == q(41, 840));
}

TEST_CASE("a misread digit is caught, and each way the proof fails says so in the Python's words", "[rk_coefficients][behaviour]") {
    const rk::Tableau& real = rk::fehlberg_table_x();
    const auto message = [](const rk::Tableau& t, std::size_t nonzero = rk::kFehlbergNonzeroT) {
        try {
            (void)rk::verify(t, nonzero);
        } catch (const rk::VerificationFailure& f) {
            return std::string(f.what());
        }
        return std::string();
    };
    CHECK(message(real).empty());

    // a coefficient of row 5 misread (1/5 as 1/6): the row no longer sums to its node
    rk::Tableau row = real;
    row.beta[5][4] = q(1, 6);
    CHECK(message(row) == "row-sum consistency fails: the transcription is wrong");

    // two coefficients of row 4 shifted against each other: the row still sums to its node, and the order conditions of c through order 7 do not hold
    rk::Tableau shifted = real;
    shifted.beta[4][2] += q(1, 100);
    shifted.beta[4][3] -= q(1, 100);
    const std::string c_failure = message(shifted);
    CHECK(c_failure.rfind("c violates ", 0) == 0);
    CHECK(c_failure.size() > std::string("c violates  order conditions through order 7").size());
    CHECK(c_failure.substr(c_failure.size() - std::string(" order conditions through order 7").size()) == " order conditions through order 7");
    const auto count_of = [](const std::string& text, const std::string& prefix) { return std::stoul(text.substr(prefix.size())); };
    const unsigned long failed = count_of(c_failure, "c violates ");
    CHECK(failed >= 1);
    CHECK(failed <= 85);
    // the same number, counted from the conditions themselves
    CHECK(rk::violations(shifted, shifted.c, 7).size() == failed);

    // only the companion weights damaged (9/35 as 9/36): c is untouched, chat violates conditions
    rk::Tableau companion = real;
    companion.chat[6] = q(9, 36);
    const std::string chat_failure = message(companion);
    CHECK(chat_failure.rfind("chat violates ", 0) == 0);
    CHECK(chat_failure.substr(chat_failure.size() - std::string(" order conditions through order 8").size()) == " order conditions through order 8");
    CHECK(count_of(chat_failure, "chat violates ") == rk::violations(companion, companion.chat, 8).size());

    // Fehlberg's printed number against the count: "forty" is what is counted, so another number must be refused, in the words of the Python
    CHECK(message(real, 39) == "c violates 40 order-8 conditions; Fehlberg's Section XV prints 39 non-zero error coefficients. These must agree.");
    CHECK(message(real, 41) == "c violates 40 order-8 conditions; Fehlberg's Section XV prints 41 non-zero error coefficients. These must agree.");

    // the checks are made in this order: the rows first
    rk::Tableau both = row;
    both.chat[6] = q(9, 36);
    CHECK(message(both) == "row-sum consistency fails: the transcription is wrong");
    // and c before chat
    rk::Tableau c_and_chat = shifted;
    c_and_chat.chat[6] = q(9, 36);
    CHECK(message(c_and_chat).rfind("c violates ", 0) == 0);
}

// ======================================================================================================================================== the generated header

TEST_CASE("emit writes the header the f-strings of the Python describe: the comment block with the counts, kStages, the five tables printed with %.17g, and the constants", "[rk_coefficients][behaviour]") {
    // an Euler tableau with made-up counts, so that every number of the text is traceable to its place
    const rk::Stats stats{1, 11, 22, 33, 44};
    const std::string text = rk::emit(euler(), stats);
    std::vector<std::string> lines;
    std::size_t from = 0;
    while (true) {
        const std::size_t nl = text.find('\n', from);
        if (nl == std::string::npos) break;
        lines.push_back(text.substr(from, nl - from));
        from = nl + 1;
    }
    CHECK(from == text.size());   // the last line ends with a newline, as "\n".join(L) + "\n" does
    REQUIRE(lines.size() == 61);   // 73 lines for the 13 rows of the real table, 12 fewer for one
    CHECK(lines[0] == "#pragma once");
    CHECK(lines[1] == "// rkf78_coefficients.hpp \xE2\x80\x94 GENERATED.  Do not edit.");
    CHECK(lines[3] == "// Produced by tools/rk_coefficients.cpp from Fehlberg, NASA TR R-287 (1968),");
    CHECK(lines[4] == "// Table X (report p.65), sha256 5553a2a3eb53785a461762cc2b29428015f1b32c3ad0a5cb57f85a421256a0c8");
    CHECK(lines[11] == "//   row-sum consistency   1/1 rows, sum_j beta_ij = alpha_i");
    CHECK(lines[12] == "//   c    through order 7  11 order conditions, 0 violated");
    CHECK(lines[13] == "//   chat through order 8  22 order conditions, 0 violated");
    CHECK(lines[14] == "//   c    at order 8       44 of 33 violated");
    CHECK(lines[17] == "// there are 33 rooted trees of order 8, exactly the range of");
    CHECK(lines[19] == "// has \"only 40 non-zero error coefficients T_v\".  Prose on one page and");
    CHECK(lines[33] == "inline constexpr int kStages = 1;");
    CHECK(lines[35] == "inline constexpr std::array<double, kStages> kAlpha = {");
    CHECK(lines[36] == "    0");
    CHECK(lines[37] == "};");
    CHECK(lines[40] == "inline constexpr std::array<std::array<double, kStages>, kStages> kBeta = {{");
    CHECK(lines[41] == "    {0},");
    CHECK(lines[42] == "}};");
    CHECK(lines[45] == "inline constexpr std::array<double, kStages> kC = {");
    CHECK(lines[46] == "    1");
    CHECK(lines[50] == "inline constexpr std::array<double, kStages> kCHat = {");
    CHECK(lines[51] == "    1");
    CHECK(lines[55] == "inline constexpr double kErrorWeight = 0.04880952380952381;");
    CHECK(lines[56] == "inline constexpr int kOrder = 7;          ///< the order that PROPAGATES");
    CHECK(lines[57] == "inline constexpr int kEstimatorOrder = 8;");
    CHECK(lines[58] == "inline constexpr int kOrder8Violations = 44;");
    CHECK(lines[60] == "}  // namespace odl::integrators::rkf78");
}

// ======================================================================================================================================== the command line and the file

namespace {

struct Root {
    TempDir td;
    fs::path path() const { return td.path(); }
};

std::string text_of_real_header() { return rk::emit(rk::fehlberg_table_x(), rk::verify(rk::fehlberg_table_x())); }

}  // namespace

TEST_CASE("without --check the tool proves the table, prints its five lines, writes the header and says so; --out is relative to --root", "[rk_coefficients][behaviour]") {
    Root r;
    fs::create_directories(r.path() / "gen");
    const Result w = run_real({"--root", r.path().string(), "--out", "gen/header.hpp"});
    CHECK(w.code == kOk);
    CHECK(w.err.empty());
    CHECK(w.out == std::string(kVerboseReal) + "wrote gen/header.hpp\n");
    CHECK(read_text(r.path() / "gen/header.hpp") == text_of_real_header());
    // the same with the options in the other form and order
    const Result again = run_real({"--out=gen/again.hpp", "--root=" + r.path().string()});
    CHECK(again.code == kOk);
    CHECK(again.out == std::string(kVerboseReal) + "wrote gen/again.hpp\n");
    CHECK(read_text(r.path() / "gen/again.hpp") == text_of_real_header());
    // an absolute --out is used as it is, whatever the root: pathlib's `ROOT / out`
    Root elsewhere;
    const std::string absolute = (elsewhere.path() / "abs.hpp").string();
    const Result abs = run_real({"--root", "/nonexistent-root", "--out", absolute});
    CHECK(abs.code == kOk);
    CHECK(abs.out == std::string(kVerboseReal) + "wrote " + absolute + "\n");
    CHECK(read_text(elsewhere.path() / "abs.hpp") == text_of_real_header());
    // it replaces what was there
    write_text(r.path() / "gen/header.hpp", "old\n");
    CHECK(run_real({"--root", r.path().string(), "--out", "gen/header.hpp"}).code == kOk);
    CHECK(read_text(r.path() / "gen/header.hpp") == text_of_real_header());
}

TEST_CASE("--check proves the table, prints nothing of it, and compares: the file it wrote passes, any change to it, a missing file and a file of another kind do not", "[rk_coefficients][behaviour]") {
    Root r;
    fs::create_directories(r.path() / "gen");
    REQUIRE(run_real({"--root", r.path().string(), "--out", "gen/header.hpp"}).code == kOk);
    const std::vector<std::string> check = {"--root", r.path().string(), "--out", "gen/header.hpp", "--check"};
    const Result ok = run_real(check);
    CHECK(ok.code == kOk);
    CHECK(ok.out == "ok  gen/header.hpp reproduces, and its order conditions hold exactly\n");
    CHECK(ok.err.empty());

    const std::string good = text_of_real_header();
    const std::string differ = "REGENERATED COEFFICIENTS DIFFER from the committed file\n";
    // one digit changed, one byte appended, one byte removed, an empty file
    std::string changed = good;
    changed[good.find("0.07407407407407407") + 5] = '8';
    for (const std::string& bad : {changed, good + "x", good.substr(0, good.size() - 1), std::string(), std::string("old\n")}) {
        write_text(r.path() / "gen/header.hpp", bad);
        const Result r2 = run_real(check);
        CHECK(r2.code == kFailed);
        CHECK(r2.out.empty());
        CHECK(r2.err == differ);
    }
    // a missing file is "differs", as the Python's `not out.exists()` was
    const Result missing = run_real({"--root", r.path().string(), "--out", "gen/none.hpp", "--check"});
    CHECK(missing.code == kFailed);
    CHECK(missing.err == differ);
    // read as Python's read_text() reads it: universal newlines, so a file with CR LF or lone CR line ends is the same text
    std::string crlf;
    for (const char ch : good) {
        if (ch == '\n') crlf += '\r';
        crlf += ch;
    }
    write_text(r.path() / "gen/header.hpp", crlf);
    CHECK(run_real(check).code == kOk);
    std::string lone_cr = good;
    std::replace(lone_cr.begin(), lone_cr.end(), '\n', '\r');
    write_text(r.path() / "gen/header.hpp", lone_cr);
    CHECK(run_real(check).code == kOk);
    // the Python died on a file it could not read; here it is refused, exit 2, naming the file: a directory, and bytes that are not UTF-8
    fs::create_directories(r.path() / "gen/dir.hpp");
    const Result dir = run_real({"--root", r.path().string(), "--out", "gen/dir.hpp", "--check"});
    CHECK(dir.code == kArgument);
    CHECK(dir.out.empty());
    CHECK(contains(dir.err, "rk_coefficients: "));
    CHECK(contains(dir.err, (r.path() / "gen/dir.hpp").string()));
    write_text(r.path() / "gen/latin.hpp", std::string("caf\xE9\n"));
    const Result latin = run_real({"--root", r.path().string(), "--out", "gen/latin.hpp", "--check"});
    CHECK(latin.code == kArgument);
    CHECK(contains(latin.err, (r.path() / "gen/latin.hpp").string()));
}

TEST_CASE("a header that cannot be written is refused, exit 2, after the five lines, naming the file", "[rk_coefficients][behaviour]") {
    Root r;
    const Result no_dir = run_real({"--root", r.path().string(), "--out", "missing/dir/header.hpp"});
    CHECK(no_dir.code == kArgument);
    CHECK(no_dir.out == kVerboseReal);       // the proof was made and printed; the write failed
    CHECK(contains(no_dir.err, "rk_coefficients: cannot write " + (r.path() / "missing/dir/header.hpp").string()));
    fs::create_directories(r.path() / "isdir");
    const Result on_dir = run_real({"--root", r.path().string(), "--out", "isdir"});
    CHECK(on_dir.code == kArgument);
    CHECK(contains(on_dir.err, "cannot write " + (r.path() / "isdir").string()));
}

TEST_CASE("a proof that fails prints only its message (the Python's sys.exit text), exit 1, and writes nothing", "[rk_coefficients][behaviour]") {
    Root r;
    rk::Tableau row = rk::fehlberg_table_x();
    row.beta[5][4] = q(1, 6);
    const Result bad = run_on(row, {"--root", r.path().string(), "--out", "header.hpp"});
    CHECK(bad.code == kFailed);
    CHECK(bad.out.empty());
    CHECK(bad.err == "row-sum consistency fails: the transcription is wrong\n");
    CHECK_FALSE(fs::exists(r.path() / "header.hpp"));
    // the same under --check, and with a file that is there
    write_text(r.path() / "header.hpp", text_of_real_header());
    const Result bad_check = run_on(row, {"--root", r.path().string(), "--out", "header.hpp", "--check"});
    CHECK(bad_check.code == kFailed);
    CHECK(bad_check.err == "row-sum consistency fails: the transcription is wrong\n");
    CHECK(bad_check.out.empty());
    // a table that is no pair of Runge-Kutta methods at all is an error the tool did not anticipate
    rk::Tableau shape = rk::fehlberg_table_x();
    shape.c.pop_back();
    const Result odd = run_on(shape, {"--root", r.path().string(), "--check"});
    CHECK(odd.code == kInternal);
    CHECK(contains(odd.err, "rk_coefficients: internal error: a tableau with 13 nodes, 13 rows of coefficients, 12 weights and 13 companion weights: the four must agree"));
}

TEST_CASE("the command line: -h, the exact option names, values, and everything else refused with the usage line, exit 2", "[rk_coefficients][behaviour]") {
    const Result help = run_real({"-h"});
    CHECK(help.code == kOk);
    CHECK(help.out.rfind(kHelpFirstLine, 0) == 0);
    CHECK(contains(help.out, "options:\n  -h, --help   show this help and exit\n  --out OUT    the header, relative to the tree (default: modules/integrators/include/odl/integrators/rkf78_coefficients.hpp)\n"));
    CHECK(contains(help.out, "  --check      verify, and compare the regenerated header with the committed one instead of writing it\n"));
    CHECK(contains(help.out, "exit codes: 0 the proof holds and the header was written or equals the committed one   1 the proof fails, or the committed header differs\n"));
    CHECK(help.err.empty());
    CHECK(run_real({"--help"}).out == help.out);
    CHECK(run_real({"--check", "--help"}).out == help.out);          // -h is acted on whatever else is given, before the proof
    const auto refused = [](const std::vector<std::string>& args, const std::string& what) {
        const Result r = run_real(args);
        INFO(what);
        CHECK(r.code == kArgument);
        CHECK(r.out.empty());
        CHECK(r.err == std::string(kHelpFirstLine) + "rk_coefficients: error: " + what + "\n");
    };
    refused({"--bogus"}, "unrecognized arguments: --bogus");
    refused({"stray"}, "unrecognized arguments: stray");
    refused({"--chec"}, "unrecognized arguments: --chec");              // no abbreviations
    refused({"--ou", "x"}, "unrecognized arguments: --ou");
    refused({"--check=yes"}, "unrecognized arguments: --check=yes");
    refused({"-h=x"}, "unrecognized arguments: -h=x");
    refused({"--out"}, "argument --out: expected one argument");
    refused({"--root"}, "argument --root: expected one argument");
    refused({"--out", "--check"}, "argument --out: expected one argument");   // a value that begins with -- is not one
    refused({"--check", "--out", "---x"}, "argument --out: expected one argument");   // nor is one that begins with three dashes (with --check, so that a defect that took it for a value would read a file, not write one)
    refused({"--root", "--out", "x"}, "argument --root: expected one argument");
    refused({"-c"}, "unrecognized arguments: -c");
    // a value that begins with a single dash is a value here (a directory called -x)
    Root r;
    fs::create_directories(r.path() / "-x");
    CHECK(run_real({"--root", r.path().string(), "--out", "-x/h.hpp"}).code == kOk);
}

TEST_CASE("an error the tool did not anticipate is reported as such, exit 70, and not as a pass or a finding", "[rk_coefficients][behaviour]") {
    Root r;
    odl::devtools_testing::ThrowingStream out;   // every write to it throws: nothing of the verification can be printed
    std::ostringstream err;
    const int code = rk::run({"--root", r.path().string(), "--out", "h.hpp"}, Streams{out, err});
    CHECK(code == kInternal);
    CHECK(err.str() == "rk_coefficients: internal error: boom\n");
}

// ======================================================================================================================================== the real tree

TEST_CASE("the committed header IS what the tool writes, byte for byte, and --check on the tree says so (ci.sh's gate 10)", "[rk_coefficients][real_tree]") {
    const fs::path root = rk::default_root();
    const std::string committed = read_text(root / kDefaultHeader);
    CHECK(committed == text_of_real_header());
    const Result gate = run_real({"--check"});
    INFO(gate.out << gate.err);
    CHECK(gate.code == kOk);
    CHECK(gate.out == std::string("ok  ") + kDefaultHeader + " reproduces, and its order conditions hold exactly\n");
    CHECK(gate.err.empty());
    // and the header says which tool made it and from what
    CHECK(contains(committed, "// Produced by tools/rk_coefficients.cpp from Fehlberg, NASA TR R-287 (1968),\n"));
    CHECK(contains(committed, std::string("sha256 ") + rk::kSourceSha256 + "\n"));
}

TEST_CASE("the report the table was read from is the one the manifest pins, and the specification names the tool that proves it", "[rk_coefficients][real_tree]") {
    const fs::path root = rk::default_root();
    const Json manifest = Json::parse(read_text(root / "manifest/manifest.json"));
    const Json* source = nullptr;
    for (const Json& e : manifest.find("entries")->as_array()) {
        if (e.find("id")->as_string() == "fehlberg-tr-r-287") source = &e;
    }
    REQUIRE(source != nullptr);
    CHECK(source->find("sha256")->as_string() == rk::kSourceSha256);
    const std::string spec = read_text(root / "spec/SPEC-integrators.md");
    CHECK(contains(spec, "tools/rk_coefficients.cpp"));
}
