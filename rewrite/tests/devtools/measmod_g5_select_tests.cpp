// tests/devtools/measmod_g5_select_tests.cpp — the G5 pass-selection rule of SPEC-measmod.md section 8.9 (plan L0 step 8, group C8): the reading of the CRD file's records, the window and the alert, the order
// of the rule and its tie-break, every refusal, the verdict of --check, the command line, and the real file against the output the Python tool recorded (L6's round9/g5_selection.out, embedded below).
// (ctests `measmod_g5_select.behaviour` and `measmod_g5_select.real_tree`; the old ctest name measmod.g5_selection_reproduces runs the tool itself.)
//
// Every expectation is DERIVED BY HAND from the statements of measmod_g5_select.py, on synthetic files whose answer is worked out in the comments, and not taken from running the port; the one exception is
// the recorded output, which is the Python's.

#include <catch2/catch_test_macros.hpp>

#include "throwing_stream.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_g5_select.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace g5 = odl::tools::measmod_g5_select;
using namespace odl::devkit;

namespace {

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

// a tree whose normal-point file has this text
struct Tree {
    TempDir td{"odl-g5"};
    fs::path root = td.path();
    explicit Tree(const std::string& np_text) {
        fs::create_directories((root / g5::kNpRelative).parent_path());
        write_text(root / g5::kNpRelative, np_text);
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const {
        std::ostringstream out;
        std::ostringstream err;
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        const int code = g5::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    }
};

std::string two(int n) { return (n < 10 ? " " : "") + std::to_string(n); }

// the words of the exception an action throws ("(nothing was thrown)" when it throws none)
template <typename Action>
std::string message_of(const Action& action) {
    try {
        action();
    } catch (const std::exception& exc) {
        return exc.what();
    }
    return "(nothing was thrown)";
}

// one session as the file writes it: H1, H2 (the station's name and its pad), H3, H4 (type, start, end, six more, the alert), H5, c0 and `points` records of type 11, one record of type 20 and one of 40
struct Spec {
    std::string name = "YARL";
    std::string pad = "7090";
    std::array<int, 6> start{2026, 1, 1, 0, 0, 0};
    std::array<int, 6> end{2026, 1, 1, 0, 10, 0};
    int alert = 0;
    int points = 3;
    std::string tag_case = "h";   // "h" or "H"
};

std::string text_of(const Spec& s) {
    std::string out;
    const std::string t = s.tag_case;
    out += t + "1 CRD  2 2026  1  1  2\n";
    out += t + "2 " + s.name + "       " + s.pad + "  5 13 3       ILRS\n";
    out += t + "3 lageos1     7603901 1155     8820 0 1 1\n";
    out += t + "4  1";
    for (const int v : s.start) out += " " + two(v);
    for (const int v : s.end) out += " " + two(v);
    out += "  0 0 0 0 1 0 2 " + std::to_string(s.alert) + "\n";
    out += t + "5  1 25 122900 SGF 08641\n";
    out += "c0 0  532.000 new la1 mcp ti1 swm met    cac\n";
    for (int i = 0; i < s.points; ++i) out += "11  4957.150591537356 0 new     1652     1639 150.4245    96314.6      9.0   15.3 na      na      na     2 2 0 3 na\n";
    out += "20  4957.150591537356 0 new\n";
    out += "40  4957.150591537356 0 new\n";
    return out;
}

Spec at(int d, int h, int mi, int s, int d2, int h2, int mi2, int s2, int alert, int points, const std::string& pad = "7090") {
    Spec spec;
    spec.pad = pad;
    spec.start = {2026, 1, d, h, mi, s};
    spec.end = {2026, 1, d2, h2, mi2, s2};
    spec.alert = alert;
    spec.points = points;
    return spec;
}

const char kUsage[] = "usage: measmod_g5_select [-h] [--check] [--root ROOT]\n";
const char kHeader[] = "        start (UTC)           end (UTC) alert  NPs  verdict\n";

// the output of `measmod_g5_select.py` on the real file, as the L6 report kept it (round9/g5_selection.out, 1,472 bytes)
const char kRecorded[] = R"GOLDEN(807 sessions in the file, 173 of pad 7090
158 of them lie outside the window (not listed)
        start (UTC)           end (UTC) alert  NPs  verdict
2026-01-01 02:07:49 2026-01-01 02:25:20     0   10  eligible
2026-01-01 05:26:45 2026-01-01 05:41:22     0    8  eligible
2026-01-01 05:48:50 2026-01-01 06:04:42     0    9  eligible
2026-01-01 17:58:53 2026-01-01 18:12:54     0    8  eligible
2026-01-01 18:17:55 2026-01-01 18:32:52     0    9  eligible
2026-01-02 04:04:45 2026-01-02 04:38:14     0   18  eligible
2026-01-02 16:32:04 2026-01-02 16:46:34     0    8  eligible
2026-01-02 16:53:06 2026-01-02 16:56:41     0    3  eligible
2026-01-03 02:48:04 2026-01-03 03:13:16     0   12  eligible
2026-01-03 06:26:37 2026-01-03 06:43:46     0    2  eligible
2026-01-03 15:23:32 2026-01-03 15:28:55     0    4  eligible
2026-01-03 15:31:26 2026-01-03 15:37:27     0    4  eligible
2026-01-03 15:44:37 2026-01-03 15:47:02     0    2  eligible
2026-01-03 18:44:39 2026-01-03 18:59:55     0    8  eligible
2026-01-03 19:01:24 2026-01-03 19:21:31     0   11  eligible

the rule's order (most normal points, ties by the earliest start):
  1. 2026-01-02 04:04:45  18 normal points
  2. 2026-01-03 02:48:04  12 normal points
  3. 2026-01-03 19:01:24  11 normal points
  4. 2026-01-01 02:07:49  10 normal points
  5. 2026-01-01 05:48:50  9 normal points
  6. 2026-01-01 18:17:55  9 normal points

CHOSEN: the session of 2026-01-02 04:04:45 UTC (YARL, pad 7090), 18 normal points
)GOLDEN";

}  // namespace

TEST_CASE("sessions: H1 begins one, H2 gives the station and the pad, H4 the times and the alert, and records of type 11 are counted; the tags are read in either case", "[measmod_g5_select][behaviour]") {
    // a line before the first H1 is ignored (the Python: `elif cur is None: continue`); an upper-case H1 begins a session as a lower-case one does; a line that BEGINS with 11 is a normal point, 110 included
    std::string text = "11 a normal point before any session, ignored\nh2 BEFORE 1234 the same\n";
    Spec a = at(1, 2, 7, 49, 1, 2, 25, 20, 0, 4);
    text += text_of(a);
    Spec b = at(2, 4, 4, 45, 2, 4, 38, 14, 1, 2, "7110");
    b.tag_case = "H";
    b.name = "ZIMM";
    text += text_of(b);
    text += "110 begins with 11, counted\n1 not 11\n\n";
    const std::vector<g5::Session> sessions = g5::sessions({});
    CHECK(sessions.empty());
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    const std::vector<g5::Session> found = g5::sessions(lines);
    REQUIRE(found.size() == 2);
    CHECK(found[0].name == "YARL");
    CHECK(found[0].pad == 7090);
    CHECK(found[0].start == g5::Time{2026, 1, 1, 2, 7, 49});
    CHECK(found[0].end == g5::Time{2026, 1, 1, 2, 25, 20});
    CHECK(found[0].alert == 0);
    CHECK(found[0].nps == 4);
    CHECK(found[1].name == "ZIMM");
    CHECK(found[1].pad == 7110);
    CHECK(found[1].start == g5::Time{2026, 1, 2, 4, 4, 45});
    CHECK(found[1].end == g5::Time{2026, 1, 2, 4, 38, 14});
    CHECK(found[1].alert == 1);
    CHECK(found[1].nps == 3);   // two records of type 11 and the line 110 that begins with 11
}

TEST_CASE("int() as Python reads it: a sign, leading zeros, digits of any script, single underscores between digits; a pad written otherwise is refused", "[measmod_g5_select][behaviour]") {
    const auto pad_of = [](const std::string& field) {
        Spec s;
        s.pad = field;
        std::vector<std::string> lines;
        std::istringstream in(text_of(s));
        for (std::string line; std::getline(in, line);) lines.push_back(line);
        return g5::sessions(lines).at(0).pad;
    };
    CHECK(pad_of("7090") == 7090);
    CHECK(pad_of("07090") == 7090);
    CHECK(pad_of("+7090") == 7090);
    CHECK(pad_of("-12") == -12);
    CHECK(pad_of("7_090") == 7090);
    CHECK(pad_of("\xD9\xA7\xD9\xA0\xD9\xA9\xD9\xA0") == 7090);   // ARABIC-INDIC DIGITS seven zero nine zero
    for (const char* bad : {"70x0", "7090.0", "_7090", "7090_", "70__90", "+", "-", "0x10"}) {
        INFO("pad field '" << bad << "'");
        // (an empty field or a blank inside one is not a bad pad: the fields shift, and the next one is read as the pad)
        std::vector<std::string> lines;
        Spec s;
        s.pad = bad;
        std::istringstream in(text_of(s));
        for (std::string line; std::getline(in, line);) lines.push_back(line);
        CHECK_THROWS_AS(g5::sessions(lines), g5::BadInput);
    }
}

TEST_CASE("format_time is %Y-%m-%d %H:%M:%S", "[measmod_g5_select][behaviour]") {
    CHECK(g5::format_time(g5::Time{2026, 1, 2, 4, 4, 45}) == "2026-01-02 04:04:45");
    CHECK(g5::format_time(g5::Time{2026, 12, 31, 23, 59, 59}) == "2026-12-31 23:59:59");
    CHECK(g5::format_time(g5::Time{5, 1, 1, 0, 0, 0}) == "0005-01-01 00:00:00");   // the year in four digits
    CHECK(g5::format_time(g5::Time{9999, 9, 9, 9, 9, 9}) == "9999-09-09 09:09:09");
    CHECK(g5::format_time(g5::Time{123, 10, 10, 10, 10, 10}) == "0123-10-10 10:10:10");
}

TEST_CASE("the rule: inside the window with the alert at 0, the most normal points, the earliest start on a tie; every Yarragadee session is listed with its verdict", "[measmod_g5_select][behaviour]") {
    // pad 7090, a window from 2026-01-01 00:00:00 to 2026-01-03 23:00:00, both ends INCLUDED:
    //   A starts exactly at the window's start, alert 0, 5 points                            inside, eligible
    //   B starts a second before it                                                          outside
    //   C ends exactly at the window's end, alert 0, 3 points                                inside, eligible
    //   D ends a second after it                                                             outside
    //   E inside, alert 1, 9 points                                                          inside, NOT eligible: "quality alert 1"
    //   F inside, alert 0, 5 points, later than A: ties with A, which is earlier             eligible
    //   G of another pad, inside, 100 points                                                 not Yarragadee: counted among the sessions of the file only
    std::string text;
    text += text_of(at(1, 0, 0, 0, 1, 0, 10, 0, 0, 5));
    Spec b = at(1, 0, 0, 0, 1, 0, 5, 0, 0, 7);
    b.start = {2025, 12, 31, 23, 59, 59};
    text += text_of(b);
    text += text_of(at(3, 22, 0, 0, 3, 23, 0, 0, 0, 3));
    text += text_of(at(3, 22, 30, 0, 3, 23, 0, 1, 0, 2));
    text += text_of(at(2, 1, 0, 0, 2, 1, 10, 0, 1, 9));
    text += text_of(at(2, 6, 0, 0, 2, 6, 10, 0, 0, 5));
    text += text_of(at(2, 8, 0, 0, 2, 8, 10, 0, 0, 100, "7110"));
    const Tree t(text);
    const Result r = t.run();
    CHECK(r.code == 0);
    CHECK(r.err.empty());
    CHECK(r.out ==
          "7 sessions in the file, 6 of pad 7090\n"
          "2 of them lie outside the window (not listed)\n" +
              std::string(kHeader) +
              "2026-01-01 00:00:00 2026-01-01 00:10:00     0    5  eligible\n"
              "2026-01-02 01:00:00 2026-01-02 01:10:00     1    9  quality alert 1\n"
              "2026-01-02 06:00:00 2026-01-02 06:10:00     0    5  eligible\n"
              "2026-01-03 22:00:00 2026-01-03 23:00:00     0    3  eligible\n"
              "\n"
              "the rule's order (most normal points, ties by the earliest start):\n"
              "  1. 2026-01-01 00:00:00  5 normal points\n"
              "  2. 2026-01-02 06:00:00  5 normal points\n"
              "  3. 2026-01-03 22:00:00  3 normal points\n"
              "\n"
              "CHOSEN: the session of 2026-01-01 00:00:00 UTC (YARL, pad 7090), 5 normal points\n"
              "  (a tie of 5 points with 2026-01-02 06:00:00, broken by the earlier start)\n");
}

TEST_CASE("the order lists six at the most, the sessions are listed by start whatever their order in the file, and a tie is only the first two", "[measmod_g5_select][behaviour]") {
    // eight eligible sessions written in the REVERSE order of their starts, with 1 to 8 points: the list is by start; the rule's order is by points, six of the eight
    std::string text;
    for (int k = 8; k >= 1; --k) text += text_of(at(1, k, 0, 0, 1, k, 30, 0, 0, k));
    const Result r = Tree(text).run();
    CHECK(r.code == 0);
    CHECK(r.out ==
          "8 sessions in the file, 8 of pad 7090\n"
          "0 of them lie outside the window (not listed)\n" +
              std::string(kHeader) +
              "2026-01-01 01:00:00 2026-01-01 01:30:00     0    1  eligible\n"
              "2026-01-01 02:00:00 2026-01-01 02:30:00     0    2  eligible\n"
              "2026-01-01 03:00:00 2026-01-01 03:30:00     0    3  eligible\n"
              "2026-01-01 04:00:00 2026-01-01 04:30:00     0    4  eligible\n"
              "2026-01-01 05:00:00 2026-01-01 05:30:00     0    5  eligible\n"
              "2026-01-01 06:00:00 2026-01-01 06:30:00     0    6  eligible\n"
              "2026-01-01 07:00:00 2026-01-01 07:30:00     0    7  eligible\n"
              "2026-01-01 08:00:00 2026-01-01 08:30:00     0    8  eligible\n"
              "\n"
              "the rule's order (most normal points, ties by the earliest start):\n"
              "  1. 2026-01-01 08:00:00  8 normal points\n"
              "  2. 2026-01-01 07:00:00  7 normal points\n"
              "  3. 2026-01-01 06:00:00  6 normal points\n"
              "  4. 2026-01-01 05:00:00  5 normal points\n"
              "  5. 2026-01-01 04:00:00  4 normal points\n"
              "  6. 2026-01-01 03:00:00  3 normal points\n"
              "\n"
              "CHOSEN: the session of 2026-01-01 08:00:00 UTC (YARL, pad 7090), 8 normal points\n");
    // a tie of the second with the third does not make a "tie" line: only the first two are compared
    std::string tied;
    tied += text_of(at(1, 1, 0, 0, 1, 1, 30, 0, 0, 9));
    tied += text_of(at(1, 2, 0, 0, 1, 2, 30, 0, 0, 4));
    tied += text_of(at(1, 3, 0, 0, 1, 3, 30, 0, 0, 4));
    const Result t = Tree(tied).run();
    CHECK(t.out.find("a tie") == std::string::npos);
    CHECK(t.out.find("CHOSEN: the session of 2026-01-01 01:00:00 UTC (YARL, pad 7090), 9 normal points\n") != std::string::npos);
}

TEST_CASE("no eligible session: the table and the order are printed, 'no eligible session' goes to the error stream, exit 1, and nothing is chosen", "[measmod_g5_select][behaviour]") {
    const Tree t(text_of(at(1, 1, 0, 0, 1, 1, 30, 0, 2, 5)) + text_of(at(4, 1, 0, 0, 4, 1, 30, 0, 0, 5)));
    const Result r = t.run();
    CHECK(r.code == 1);
    CHECK(r.out ==
          "2 sessions in the file, 2 of pad 7090\n"
          "1 of them lie outside the window (not listed)\n" +
              std::string(kHeader) +
              "2026-01-01 01:00:00 2026-01-01 01:30:00     2    5  quality alert 2\n"
              "\n"
              "the rule's order (most normal points, ties by the earliest start):\n");
    CHECK(r.err == "no eligible session\n");
    // the same with --check: the verdict is not reached
    const Result c = t.run({"--check"});
    CHECK(c.code == 1);
    CHECK(c.out == r.out);
    CHECK(c.err == "no eligible session\n");
    // a file with no session of the pad at all
    const Result none = Tree(text_of(at(1, 1, 0, 0, 1, 1, 30, 0, 0, 5, "7110"))).run();
    CHECK(none.code == 1);
    CHECK(none.out.rfind("1 sessions in the file, 0 of pad 7090\n0 of them lie outside the window (not listed)\n", 0) == 0);
    // an empty file
    const Result empty = Tree("").run();
    CHECK(empty.code == 1);
    CHECK(empty.out.rfind("0 sessions in the file, 0 of pad 7090\n", 0) == 0);
}

TEST_CASE("--check: the choice recorded in SPEC-measmod.md 8.9 is 2026-01-02 04:04:45 with 18 points, and any other is FAILED with what the rule chose", "[measmod_g5_select][behaviour]") {
    const std::string recorded_choice = text_of(at(2, 4, 4, 45, 2, 4, 38, 14, 0, 18)) + text_of(at(2, 6, 0, 0, 2, 6, 10, 0, 0, 17));
    const Result ok = Tree(recorded_choice).run({"--check"});
    CHECK(ok.code == 0);
    CHECK(ok.out.size() > 43);
    CHECK(ok.out.substr(ok.out.size() - 43) == "ok       the recorded choice is reproduced\n");
    CHECK(ok.err.empty());
    // a different number of points, a different start, each on its own
    for (const std::string& damaged : {text_of(at(2, 4, 4, 45, 2, 4, 38, 14, 0, 19)), text_of(at(2, 4, 4, 45, 2, 4, 38, 14, 0, 17)), text_of(at(2, 4, 4, 46, 2, 4, 38, 14, 0, 18)),
                                       text_of(at(2, 4, 4, 44, 2, 4, 38, 14, 0, 18)), text_of(at(1, 4, 4, 45, 1, 4, 38, 14, 0, 18))}) {
        const Result r = Tree(damaged).run({"--check"});
        CHECK(r.code == 1);
        const std::string tail = r.out.substr(r.out.rfind("FAILED"));
        CHECK(tail.rfind("FAILED   the rule now chooses ", 0) == 0);
        CHECK(tail.find(" points, not ('2026-01-02 04:04:45', 18)\n") != std::string::npos);
        CHECK(r.out.find("ok       the recorded") == std::string::npos);
    }
    // the text of the failure: the start and the points the rule chose
    const Result r = Tree(text_of(at(2, 4, 4, 46, 2, 4, 38, 14, 0, 19))).run({"--check"});
    CHECK(r.out.substr(r.out.rfind("FAILED")) == "FAILED   the rule now chooses 2026-01-02 04:04:46 with 19 points, not ('2026-01-02 04:04:45', 18)\n");
    // without --check the choice is printed and not judged
    const Result plain = Tree(text_of(at(2, 4, 4, 46, 2, 4, 38, 14, 0, 19))).run();
    CHECK(plain.code == 0);
    CHECK(plain.out.find("FAILED") == std::string::npos);
    CHECK(plain.out.find("ok  ") == std::string::npos);
}

TEST_CASE("an input that is missing or is not the shape expected is refused, exit 2, naming the line or the file", "[measmod_g5_select][behaviour]") {
    const auto refused = [](const std::string& text, const std::string& what) {
        const Result r = Tree(text).run();
        INFO(what);
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.find("measmod_g5_select: ") == 0);
        CHECK(r.err.find(" (run `tools/bootstrap.sh`)\n") != std::string::npos);
        return r.err;
    };
    // an H2 with too few fields, an H4 with too few, a field that is no integer, a month, a day, an hour, a minute, a second, a year that is no date
    CHECK(refused("h1 CRD\nh2 YARL\n", "h2 short").find("line 2: an H2 record has 2 fields, three are wanted") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 7 49\n", "h4 short").find("line 3: an H4 record has 8 fields, fourteen are wanted") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 7 49 2026 1 1 2 25 xx 0 0 0 0 1 0 2 0\n", "h4 text").find("line 3: a time field of an H4 record: 'xx' is not an integer") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 13 1 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "month").find("line 3: the start of an H4 record: month 13 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 2 30 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "day").find("day 30 is out of range for the month") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 24 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "hour").find("hour 24 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 60 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "minute").find("minute 60 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 7 60 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "second").find("second 60 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 0 1 1 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "year").find("year 0 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 7 49 10000 1 1 2 25 20 0 0 0 0 1 0 2 0\n", "year 10000").find("the end of an H4 record: year 10000 is out of range") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 7090\nh4 1 2026 1 1 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 zz\n", "alert").find("the last field of an H4 record: 'zz' is not an integer") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL seven\n", "pad").find("line 2: the pad of an H2 record: 'seven' is not an integer") != std::string::npos);
    CHECK(refused("h1 CRD\nh2 YARL 99999999999999999999\n", "pad range").find("is beyond the range this reads") != std::string::npos);
    // a leap day is a date, in a leap year only (a valid date outside the window is no refusal: nothing is eligible, exit 1)
    CHECK(Tree(text_of(at(1, 0, 0, 0, 1, 0, 1, 0, 0, 1))).run().code == 0);
    Spec leap;
    leap.start = {2024, 2, 29, 0, 0, 0};
    leap.end = {2024, 2, 29, 0, 1, 0};
    CHECK(Tree(text_of(leap)).run().code == 1);
    leap.start = {2026, 2, 29, 0, 0, 0};
    CHECK(Tree(text_of(leap)).run().code == 2);
    leap.start = {1900, 2, 29, 0, 0, 0};   // 1900 is no leap year
    CHECK(Tree(text_of(leap)).run().code == 2);
    leap.start = {2000, 2, 29, 0, 0, 0};   // 2000 is
    CHECK(Tree(text_of(leap)).run().code == 1);
    // a session of the pad with no H4 (it lies in no window and has no alert): refused, naming the pad; a session of another pad with no H4 is not read
    {
        const Result r = Tree("h1 CRD\nh2 YARL 7090\n").run();
        CHECK(r.code == 2);
        CHECK(r.err == "measmod_g5_select: a session of pad 7090 has no H4 record (run `tools/bootstrap.sh`)\n");
        CHECK(Tree("h1 CRD\nh2 ZIMM 7810\n").run().code == 1);
    }
    // a file that does not exist, a directory in its place, a file that is not UTF-8
    {
        TempDir empty("odl-g5-empty");
        std::ostringstream out;
        std::ostringstream err;
        CHECK(g5::run({"--root", empty.path().string()}, Streams{out, err}) == 2);
        CHECK(err.str().find("measmod_g5_select: ") == 0);
        CHECK(err.str().find(g5::kNpRelative) != std::string::npos);
        CHECK(err.str().find(" (run `tools/bootstrap.sh`)\n") != std::string::npos);
        CHECK(out.str().empty());
        fs::create_directories(empty.path() / g5::kNpRelative);
        std::ostringstream out2;
        std::ostringstream err2;
        CHECK(g5::run({"--root", empty.path().string()}, Streams{out2, err2}) == 2);
        CHECK(err2.str().find(g5::kNpRelative) != std::string::npos);
    }
    {
        Tree t("h1 CRD\n");
        write_text(t.root / g5::kNpRelative, std::string("h1 CRD \xFF\xFE\n"));
        const Result r = t.run();
        CHECK(r.code == 2);
        CHECK(r.err.find(g5::kNpRelative) != std::string::npos);
    }
}

TEST_CASE("the command line: --check, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[measmod_g5_select][behaviour]") {
    const Tree t(text_of(at(2, 4, 4, 45, 2, 4, 38, 14, 0, 18)));
    const auto run_args = [](const std::vector<std::string>& args) {
        std::ostringstream out;
        std::ostringstream err;
        const int code = g5::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    };
    const std::string help =
        std::string(kUsage) +
        "\n"
        "The G5 pass-selection rule of SPEC-measmod.md section 8.9, applied to the normal-point file's metadata: among the CRD sessions of Yarragadee (pad 7090) inside the window with the data-quality-alert\n"
        "indicator 0, the one with the most normal points, ties broken by the earliest start.  The tool prints every Yarragadee session of the file with the rule's verdict.\n"
        "\n"
        "options:\n"
        "  -h, --help   show this help and exit\n"
        "  --check      exit 1 unless the chosen session is the one recorded in SPEC-measmod.md section 8.9\n"
        "  --root ROOT  the tree whose data/cache holds the file (default: the tree this tool was built from)\n"
        "\n"
        "exit codes: 0 printed (and, with --check, the choice reproduced)   1 the choice differs, or no session is eligible\n"
        "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_args({flag});
        CHECK(r.code == 0);
        CHECK(r.out == help);
        CHECK(r.err.empty());
    }
    CHECK(run_args({"--root=" + t.root.string(), "--check"}).code == 0);
    CHECK(run_args({"--check", "--root", t.root.string()}).code == 0);
    CHECK(run_args({"-h", "--frobnicate"}).code == 0);
    CHECK(run_args({"--frobnicate", "-h"}).code == 2);
    const Result unknown = run_args({"--frobnicate"});
    CHECK(unknown.code == 2);
    CHECK(unknown.err == std::string(kUsage) + "measmod_g5_select: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    CHECK(run_args({"stray"}).code == 2);
    CHECK(run_args({"--check=1"}).code == 2);
    const Result bare = run_args({"--root"});
    CHECK(bare.code == 2);
    CHECK(bare.err == std::string(kUsage) + "measmod_g5_select: error: argument --root: expected one argument\n");
    // an argument that looks like an option is not the value of --root (the refusal says so; a root taken from it would be refused as a missing file, with another text); one dash is a value
    for (const char* option_like : {"--check", "---x", "--other"}) {
        const Result r = run_args({"--root", option_like});
        CHECK(r.code == 2);
        CHECK(r.err == std::string(kUsage) + "measmod_g5_select: error: argument --root: expected one argument\n");
        CHECK(r.out.empty());
    }
    {
        const Result r = run_args({"--root", "-x"});
        CHECK(r.code == 2);
        CHECK(r.err.find("expected one argument") == std::string::npos);
        CHECK(r.err.find("-x/") != std::string::npos);   // the file looked for is under the root "-x"
    }
    // the default root is the tree this tool was built from
    CHECK(g5::default_settings().root == g5::default_root());
}

TEST_CASE("sessions: the edges of what is read -- a record that is only its tag, an H4 of exactly fourteen fields, integers at the edge of the range, dates at the edge of datetime's", "[measmod_g5_select][behaviour]") {
    // (rule 5, group C8: each of these was found by a defect that no test of the first round noticed)
    // a record that is nothing but its tag (two characters) IS that record: an H1 begins a session, in either case; an H2 or an H4 of one field is refused with its count
    CHECK(g5::sessions({"H1"}).size() == 1);
    CHECK(g5::sessions({"h1"}).size() == 1);
    CHECK(g5::sessions({"H1", "H1", "H1"}).size() == 3);
    CHECK(message_of([] { (void)g5::sessions({"H1", "H2"}); }) == "line 2: an H2 record has 1 fields, three are wanted");
    CHECK(message_of([] { (void)g5::sessions({"H1", "h2"}); }) == "line 2: an H2 record has 1 fields, three are wanted");
    CHECK(message_of([] { (void)g5::sessions({"H1", "H4"}); }) == "line 2: an H4 record has 1 fields, fourteen are wanted");
    CHECK(message_of([] { (void)g5::sessions({"H1", "h4"}); }) == "line 2: an H4 record has 1 fields, fourteen are wanted");
    CHECK(message_of([] { (void)g5::sessions({"H1", "H4 1 2026 1 1 0 0 0 2026 1 1 0 10"}); }) == "line 2: an H4 record has 13 fields, fourteen are wanted");
    // an H4 of EXACTLY fourteen fields is read: the tag, the type, twelve time fields, and the last of them is also the alert (int(f[-1]) of the Python)
    {
        const std::vector<g5::Session> found = g5::sessions({"H1", "H4 1 2026 1 1 0 0 0 2026 1 1 0 10 5"});
        REQUIRE(found.size() == 1);
        CHECK(found[0].start == g5::Time{2026, 1, 1, 0, 0, 0});
        CHECK(found[0].end == g5::Time{2026, 1, 1, 0, 10, 5});
        CHECK(found[0].alert == 5);
    }
    // and one of fifteen has the alert after the twelve
    {
        const std::vector<g5::Session> found = g5::sessions({"H1", "H4 1 2026 1 1 0 0 0 2026 1 1 0 10 5 7"});
        REQUIRE(found.size() == 1);
        CHECK(found[0].end == g5::Time{2026, 1, 1, 0, 10, 5});
        CHECK(found[0].alert == 7);
    }
    // the pad is an int64: the largest and the negative of it are read, the next one is beyond the range this reads, as are longer ones
    const auto pad_of = [](const char* field) { return g5::sessions({"H1", std::string("H2 YARL ") + field}).at(0).pad; };
    CHECK(pad_of("9223372036854775807") == std::numeric_limits<std::int64_t>::max());
    CHECK(pad_of("-9223372036854775807") == -std::numeric_limits<std::int64_t>::max());
    CHECK(pad_of("0000000000000000000000000000007090") == 7090);
    CHECK(message_of([] { (void)g5::sessions({"H1", "H2 YARL 9223372036854775808"}); }) == "line 2: the pad of an H2 record: '9223372036854775808' is beyond the range this reads");
    CHECK(message_of([] { (void)g5::sessions({"H1", "H2 YARL 92233720368547758070"}); }) == "line 2: the pad of an H2 record: '92233720368547758070' is beyond the range this reads");
    CHECK(message_of([] { (void)g5::sessions({"H1", "H2 YARL 99999999999999999999"}); }) == "line 2: the pad of an H2 record: '99999999999999999999' is beyond the range this reads");
    // datetime's own limits, for the start and for the end of an H4: years 1 to 9999, months 1 to 12, the days of the month (February of a leap year has 29), hours 0 to 23, minutes and seconds 0 to 59
    const auto h4 = [](const std::array<int, 6>& start, const std::array<int, 6>& end) {
        std::string line = "H4 1";
        for (const int v : start) line += " " + std::to_string(v);
        for (const int v : end) line += " " + std::to_string(v);
        return line + " 0";
    };
    const std::array<int, 6> fine = {2026, 1, 1, 0, 0, 0};
    const auto start_of = [&](const std::array<int, 6>& s) { return g5::sessions({"H1", h4(s, fine)}).at(0).start; };
    CHECK(start_of({1, 1, 1, 0, 0, 0}) == g5::Time{1, 1, 1, 0, 0, 0});
    CHECK(start_of({9999, 12, 31, 23, 59, 59}) == g5::Time{9999, 12, 31, 23, 59, 59});
    CHECK(start_of({2024, 2, 29, 0, 0, 0}) == g5::Time{2024, 2, 29, 0, 0, 0});
    CHECK(start_of({2000, 2, 29, 0, 0, 0}) == g5::Time{2000, 2, 29, 0, 0, 0});
    CHECK(start_of({2026, 4, 30, 0, 0, 0}) == g5::Time{2026, 4, 30, 0, 0, 0});
    CHECK(start_of({2026, 1, 31, 0, 0, 0}) == g5::Time{2026, 1, 31, 0, 0, 0});
    CHECK(start_of({2026, 12, 31, 0, 0, 0}) == g5::Time{2026, 12, 31, 0, 0, 0});
    CHECK(start_of({2026, 2, 28, 0, 0, 0}) == g5::Time{2026, 2, 28, 0, 0, 0});
    const auto refusal = [&](const std::array<int, 6>& s, const char* which = "start") {
        return message_of([&] { (void)g5::sessions({"H1", std::string(which) == "start" ? h4(s, fine) : h4(fine, s)}); });
    };
    const std::string where = "line 2: the start of an H4 record: ";
    CHECK(refusal({0, 1, 1, 0, 0, 0}) == where + "year 0 is out of range");
    CHECK(refusal({-1, 1, 1, 0, 0, 0}) == where + "year -1 is out of range");
    CHECK(refusal({10000, 1, 1, 0, 0, 0}) == where + "year 10000 is out of range");
    CHECK(refusal({2026, 0, 1, 0, 0, 0}) == where + "month 0 is out of range");
    CHECK(refusal({2026, 13, 1, 0, 0, 0}) == where + "month 13 is out of range");
    CHECK(refusal({2026, 1, 0, 0, 0, 0}) == where + "day 0 is out of range for the month");
    CHECK(refusal({2026, 1, 32, 0, 0, 0}) == where + "day 32 is out of range for the month");
    CHECK(refusal({2026, 4, 31, 0, 0, 0}) == where + "day 31 is out of range for the month");
    CHECK(refusal({2026, 2, 29, 0, 0, 0}) == where + "day 29 is out of range for the month");
    CHECK(refusal({1900, 2, 29, 0, 0, 0}) == where + "day 29 is out of range for the month");
    CHECK(refusal({2024, 2, 30, 0, 0, 0}) == where + "day 30 is out of range for the month");
    CHECK(refusal({2026, 1, 1, 24, 0, 0}) == where + "hour 24 is out of range");
    CHECK(refusal({2026, 1, 1, -1, 0, 0}) == where + "hour -1 is out of range");
    CHECK(refusal({2026, 1, 1, 0, 60, 0}) == where + "minute 60 is out of range");
    CHECK(refusal({2026, 1, 1, 0, -1, 0}) == where + "minute -1 is out of range");
    CHECK(refusal({2026, 1, 1, 0, 0, 60}) == where + "second 60 is out of range");
    CHECK(refusal({2026, 1, 1, 0, 0, -1}) == where + "second -1 is out of range");
    CHECK(refusal({2026, 13, 1, 0, 0, 0}, "end") == "line 2: the end of an H4 record: month 13 is out of range");
    CHECK(refusal({0, 1, 1, 0, 0, 0}, "end") == "line 2: the end of an H4 record: year 0 is out of range");
}

TEST_CASE("sessions that start at the same second keep the order the file has them in: in the listing, and in the order of the rule", "[measmod_g5_select][behaviour]") {
    // the Python's sort is stable and its key is the start (the order of the rule: the points, then the start): equal keys stay in the file's order
    Spec first = at(1, 2, 7, 49, 1, 2, 25, 20, 0, 3);
    first.name = "AAAA";
    Spec second = at(1, 2, 7, 49, 1, 2, 25, 20, 0, 5);
    second.name = "BBBB";
    {
        const Tree t(text_of(first) + text_of(second));
        const Result r = t.run();
        CHECK(r.code == 0);
        CHECK(r.err.empty());
        CHECK(r.out ==
              "2 sessions in the file, 2 of pad 7090\n"
              "0 of them lie outside the window (not listed)\n" +
                  std::string(kHeader) +
                  "2026-01-01 02:07:49 2026-01-01 02:25:20     0    3  eligible\n"
                  "2026-01-01 02:07:49 2026-01-01 02:25:20     0    5  eligible\n"
                  "\n"
                  "the rule's order (most normal points, ties by the earliest start):\n"
                  "  1. 2026-01-01 02:07:49  5 normal points\n"
                  "  2. 2026-01-01 02:07:49  3 normal points\n"
                  "\n"
                  "CHOSEN: the session of 2026-01-01 02:07:49 UTC (BBBB, pad 7090), 5 normal points\n");
    }
    // the same points and the same start: the first of the file is the one chosen, and the tie is said
    first.points = 4;
    second.points = 4;
    {
        const Tree t(text_of(first) + text_of(second));
        const Result r = t.run();
        CHECK(r.code == 0);
        CHECK(r.out ==
              "2 sessions in the file, 2 of pad 7090\n"
              "0 of them lie outside the window (not listed)\n" +
                  std::string(kHeader) +
                  "2026-01-01 02:07:49 2026-01-01 02:25:20     0    4  eligible\n"
                  "2026-01-01 02:07:49 2026-01-01 02:25:20     0    4  eligible\n"
                  "\n"
                  "the rule's order (most normal points, ties by the earliest start):\n"
                  "  1. 2026-01-01 02:07:49  4 normal points\n"
                  "  2. 2026-01-01 02:07:49  4 normal points\n"
                  "\n"
                  "CHOSEN: the session of 2026-01-01 02:07:49 UTC (AAAA, pad 7090), 4 normal points\n"
                  "  (a tie of 4 points with 2026-01-01 02:07:49, broken by the earlier start)\n");
    }
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[measmod_g5_select][behaviour]") {
    const Tree t(text_of(at(2, 4, 4, 45, 2, 4, 38, 14, 0, 18)));
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(g5::run({"--root", t.root.string()}, Streams{out, err}) == 70);
    CHECK(err.str() == "measmod_g5_select: internal error: boom\n");
}

TEST_CASE("on the real normal-point file the output is, byte for byte, what the Python tool recorded (round9/g5_selection.out), and --check adds one line", "[measmod_g5_select][real_tree]") {
    const std::string recorded = kRecorded;
    REQUIRE(recorded.size() == 1472);
    std::ostringstream out;
    std::ostringstream err;
    CHECK(g5::run({}, Streams{out, err}) == 0);
    CHECK(err.str().empty());
    CHECK(out.str() == recorded);
    std::ostringstream out2;
    std::ostringstream err2;
    CHECK(g5::run({"--check"}, Streams{out2, err2}) == 0);
    CHECK(err2.str().empty());
    CHECK(out2.str() == recorded + "ok       the recorded choice is reproduced\n");
}
