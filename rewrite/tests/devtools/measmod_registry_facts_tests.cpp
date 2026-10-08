// tests/devtools/measmod_registry_facts_tests.cpp — the registry's data facts, counted independently of the registry (SPEC-measmod.md MEAS-A-004 / MEAS-A-010; plan L0 step 8, group C8): every count on
// synthetic SINEX files whose answers are worked out in the comments, the SINEX epochs and the gaps between solutions, the header's own count, the post-seismic events, the printed text and the verdict, every
// refusal, the command line, and the real files against the output predicted by an independent computation in awk made before the port existed (embedded below).
// (ctests `measmod_registry_facts.behaviour` and `measmod_registry_facts.real_tree`; the old ctest name measmod.registry_facts_reproduce runs the tool itself.)
//
// Every expectation is DERIVED BY HAND from the statements of measmod_registry_facts.py; the real tree's is the awk control's output (C8_registered/registry_control.awk), which was written from the
// definitions and shares no code with the tool.

#include <catch2/catch_test_macros.hpp>

#include "throwing_stream.hpp"

#include <odl/devkit/fs.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_registry_facts.hpp"

#include <array>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace rf = odl::tools::measmod_registry_facts;
using namespace odl::devkit;

namespace {

struct Result {
    int code = -1;
    std::string out;
    std::string err;
};

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    return lines;
}

// a SITE/ID line: the code in columns 2-5, the point in column 8, the DOMES number in columns 10-18, and the SOD last
std::string site_line(const std::string& code, const std::string& point, const std::string& domes, const std::string& sod) {
    return " " + code + "  " + point + " " + domes + " L Name       XXXX FIXED   13  3 55.0  52 22 48.9   148.5     " + sod + "\n";
}

// a SOLUTION/EPOCHS row: the marker, the solution number as it is written, the type, the start and the end of the data, and a mean epoch
std::string epoch_row(const std::string& code, const std::string& point, const std::string& soln, const std::string& start, const std::string& end) {
    return " " + code + "  " + point + "    " + soln + " C " + start + " " + end + " 10:001:00000\n";
}

std::string epoch_line(const std::string& code, const std::string& point, int soln, const std::string& start, const std::string& end) {
    return epoch_row(code, point, std::to_string(soln), start, end);
}

// the SLRF2020 text of one marker, 1000 A, with these solutions (number as written, start, end) in this order
std::string one_marker(const std::vector<std::array<std::string, 3>>& solutions) {
    std::string slrf = "+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/ID\n+SOLUTION/EPOCHS\n";
    for (const auto& s : solutions) slrf += epoch_row("1000", "A", s[0], s[1], s[2]);
    return slrf + "-SOLUTION/EPOCHS\n";
}

template <class Action>
std::string message_of(const Action& action) {
    try {
        action();
    } catch (const std::exception& error) {
        return error.what();
    }
    return "(no exception)";
}

const char kHeaderLine[] = "* The archive contains positions and velocities for 3 unique sites.\n";

// the SLRF2020 file of the worked example: six SITE/ID rows (five pads, six markers), nine epochs (five markers with a solution, three with more than one, one gap)
std::string slrf_text() {
    std::string t = "%=SNX 2.02 ILRS\n";
    t += kHeaderLine;
    t += "+SITE/ID\n*CODE PT __DOMES__ T _STATION DESCRIPTION__ APPROX_LON_ APPROX_LAT_ _APP_H_     _SOD#___\n";
    t += site_line("1000", "A", "10000S001", "10000001");
    t += site_line("1000", "B", "10000S002", "10000002");
    t += site_line("2000", "A", "20000S001", "20000001");
    t += site_line("3000", "A", "30000S001", "30000001");
    t += site_line("7329", "A", "73290S001", "73290001");
    t += site_line("7317", "A", "73170S001", "73170001");
    t += "-SITE/ID\n+SOLUTION/EPOCHS\n*Code PT SOLN T Data_start__ Data_end____ Mean_epoch__\n";
    t += epoch_line("1000", "A", 1, "90:001:00000", "99:365:86399");   // ends at the end of 1999-12-31 = 2000-01-01 00:00:00 by the file's day-end convention
    t += epoch_line("1000", "A", 2, "00:001:00000", "00:000:00000");   // starts there, and is open: the one starts where the other ends, which is no gap
    t += epoch_line("1000", "B", 1, "90:001:00000", "00:000:00000");
    t += epoch_line("2000", "A", 2, "96:001:00000", "00:000:00000");   // listed BEFORE solution 1: the order of the numbers decides
    t += epoch_line("2000", "A", 1, "90:001:00000", "95:100:00000");   // ends 1995-04-10 00:00:00, and solution 2 starts 1996-01-01: a gap
    t += epoch_line("7329", "A", 1, "00:000:00000", "10:365:86399");   // an unknown start
    t += epoch_line("7317", "A", 1, "90:001:00000", "00:000:00000");   // an open end: no gap with what follows
    t += epoch_line("7317", "A", 2, "10:001:00000", "20:001:00000");
    t += epoch_line("7317", "A", 3, "00:000:00000", "30:001:00000");   // an unknown start: no gap with what precedes
    t += "-SOLUTION/EPOCHS\n";
    return t;
}

// the eccentricity file: ten SITE/ID rows, eight distinct SODs, one identical duplicate (20000001), one that is not (30000001), one whose DOMES disagrees (10000002), one on a pad SLRF2020 lacks
std::string ecc_text() {
    std::string t = "%=SNX 2.02 ILRS\n+SITE/ID\n*Code PT __DOMES__ T _STATION DESCRIPTION__ APPROX_LON_ APPROX_LAT_ _APP_H_     CDP-SOD_\n";
    t += site_line("1000", "A", "10000S001", "10000001");
    t += site_line("1000", "B", "10000S099", "10000002");
    t += site_line("2000", "A", "20000S001", "20000001");
    t += site_line("2000", "A", "20000S001", "20000001");
    t += site_line("2000", "A", "20000S002", "20000002");
    t += site_line("3000", "A", "30000S001", "30000001");
    t += site_line("3000", "A", "30000S009", "30000001");
    t += site_line("4000", "A", "40000S001", "40000001");
    t += site_line("7329", "A", "73290S001", "73290001");
    t += site_line("7317", "A", "73170S001", "73170001");
    t += "-SITE/ID\n";
    return t;
}

// the post-seismic file: site 7110 twice (its two epochs are one site's), 7237, 7600, and four lines that are not events (one colon, three colons, a site that is no number, a line with three fields)
std::string psd_text() {
    return " 7110  A 40497M001 10:094:81643 E 1   -3.67  0.0467                     SLR\n"
           "                                N 0\n"
           "                                U 0\n"
           " 7237  A 21611S001 11:070:20783 E 2    9.32  0.6549                     SLR\n"
           "                                N 2   -3.38  0.5433\n"
           " 7110  A 40497M001 12:001:00000 E 1    1.00  0.0100                     SLR\n"
           " 7500  A 50000M001 1:2 E 1    1.00  0.0100\n"
           " 7501  A 50000M001 13:001:00000:99 E 1    1.00  0.0100\n"
           " abc  A 50000M001 13:001:00000 E 1    1.00  0.0100\n"
           " 7502  A 50000M001\n"
           " 7600  A 60000M001 14:001:00000 E 1    1.00  0.0100                     SLR\n";
}

struct Tree {
    TempDir td{"odl-rf"};
    fs::path root = td.path();
    Tree(const std::string& slrf, const std::string& ecc, const std::string& psd) {
        put(rf::kSlrfRelative, slrf);
        put(rf::kEccRelative, ecc);
        put(rf::kPsdRelative, psd);
    }
    void put(const char* relative, const std::string& text) const {
        fs::create_directories((root / relative).parent_path());
        write_text(root / relative, text);
    }
    [[nodiscard]] Result run(const std::vector<std::string>& extra = {}) const {
        std::ostringstream out;
        std::ostringstream err;
        std::vector<std::string> args = {"--root", root.string()};
        args.insert(args.end(), extra.begin(), extra.end());
        const int code = rf::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    }
};

std::int64_t value_of(const rf::Facts& facts, const std::string& name) {
    for (const auto& entry : facts.counts) {
        if (entry.first == name) return entry.second;
    }
    FAIL("no count named " << name);
    return -999;
}

const char kUsage[] = "usage: measmod_registry_facts [-h] [--root ROOT] [--check]\n";

// what the awk control (C8_registered/registry_control.awk: written from the definitions, before this tool) printed for the real files, `--check` included
const char kControl[] =
    "  slrf_site_id_rows                       483\n"
    "  slrf_distinct_sods                      483\n"
    "  slrf_pads                               186\n"
    "  slrf_markers                            190\n"
    "  slrf_markers_with_solution              189\n"
    "  slrf_solutions                          237\n"
    "  slrf_markers_with_more_than_one_soln     28\n"
    "  slrf_gaps_between_solutions              48\n"
    "  slrf_header_sites                       184\n"
    "  pads_added_after_header                   2\n"
    "  ecc_site_id_rows                        543\n"
    "  ecc_distinct_sods                       542\n"
    "  ecc_pads                                235\n"
    "  ecc_identical_duplicate_rows              1\n"
    "  pad_point_domes_disagreements             0\n"
    "  placed_sods                             482\n"
    "  unplaced_sods                            60\n"
    "  unplaced_pad_absent                      57\n"
    "  unplaced_sod_absent_on_known_pad          2\n"
    "  unplaced_marker_without_solution          1\n"
    "  psd_sites                                 8\n"
    "  psd_events                               12\n"
    "  PSD events (site: SINEX epochs): {'7110': ['10:094:81643'], '7237': ['11:070:20783'], '7308': ['11:070:20783'], '7328': ['11:070:20783'], '7358': ['04:361:03529', '19:008:45570'], '7403': ['01:174:73993', '01:188:34724'], '7405': ['10:058:23656', '11:042:72331'], '7838': ['11:070:20783', '04:249:36428']}\n"
    "  unplaced SODs on pads SLRF2020 lists: ['73071701', '73071702', '73071703']\n"
    "ok       every pinned number matches\n";

}  // namespace

TEST_CASE("the counts of the worked example: SLRF2020's rows, pads, markers, solutions and gaps", "[measmod_registry_facts][behaviour]") {
    const rf::Facts facts = rf::count(lines_of(slrf_text()), lines_of(ecc_text()), lines_of(psd_text()));
    // six SITE/ID rows with six distinct SODs, on five pads (1000 2000 3000 7329 7317) and six markers (1000A 1000B 2000A 3000A 7329A 7317A)
    CHECK(value_of(facts, "slrf_site_id_rows") == 6);
    CHECK(value_of(facts, "slrf_distinct_sods") == 6);
    CHECK(value_of(facts, "slrf_pads") == 5);
    CHECK(value_of(facts, "slrf_markers") == 6);
    // nine epoch rows of five markers (1000A 1000B 2000A 7329A 7317A), three of which have more than one (1000A, 2000A, 7317A); one gap: 2000A's solution 1 ends 1995-04-10 and 2 starts 1996-01-01.  1000A's
    // two meet exactly (no gap), 7317A's pairs each have an unknown end or start (no gap)
    CHECK(value_of(facts, "slrf_solutions") == 9);
    CHECK(value_of(facts, "slrf_markers_with_solution") == 5);
    CHECK(value_of(facts, "slrf_markers_with_more_than_one_soln") == 3);
    CHECK(value_of(facts, "slrf_gaps_between_solutions") == 1);
    // the header says 3 unique sites; the file has 5 pads, and removing 7329 and 7317 leaves 3: two pads were added after the sentence was written
    CHECK(value_of(facts, "slrf_header_sites") == 3);
    CHECK(value_of(facts, "pads_added_after_header") == 2);
}

TEST_CASE("the counts of the worked example: the eccentricity file, what is placed and what is not", "[measmod_registry_facts][behaviour]") {
    const rf::Facts facts = rf::count(lines_of(slrf_text()), lines_of(ecc_text()), lines_of(psd_text()));
    // ten rows, eight distinct SODs (10000001 10000002 20000001 20000002 30000001 40000001 73290001 73170001), six pads (1000 2000 3000 4000 7329 7317); 20000001 is listed twice with one key: one identical duplicate
    // (30000001 twice with two keys is a duplicate that is not identical: not counted)
    CHECK(value_of(facts, "ecc_site_id_rows") == 10);
    CHECK(value_of(facts, "ecc_distinct_sods") == 8);
    CHECK(value_of(facts, "ecc_pads") == 6);
    CHECK(value_of(facts, "ecc_identical_duplicate_rows") == 1);
    // the SODs of both files whose (pad, point, DOMES) differ: 10000002 (the DOMES) and 30000001 (the later row's DOMES replaces the earlier)
    CHECK(value_of(facts, "pad_point_domes_disagreements") == 2);
    // placed: 10000001 20000001 73290001 73170001 (in SLRF2020 with the same key and a marker with a solution); the other four are not: 10000002 (key), 20000002 (not in SLRF2020, on a pad it lists), 30000001
    // (key, and its marker has no solution), 40000001 (a pad it lacks)
    CHECK(value_of(facts, "placed_sods") == 4);
    CHECK(value_of(facts, "unplaced_sods") == 4);
    CHECK(value_of(facts, "unplaced_pad_absent") == 1);
    CHECK(value_of(facts, "unplaced_sod_absent_on_known_pad") == 1);
    CHECK(value_of(facts, "unplaced_marker_without_solution") == 1);
    // the unplaced SODs on the pads SLRF2020 lists, sorted as strings
    CHECK(facts.unplaced_on_known_pads == std::vector<std::string>{"10000002", "20000002", "30000001"});
}

TEST_CASE("the post-seismic events: a site is its first field when that is made of digits, and an event its fourth field when it has two colons; sites in the order they first appear", "[measmod_registry_facts][behaviour]") {
    const rf::Facts facts = rf::count(lines_of(slrf_text()), lines_of(ecc_text()), lines_of(psd_text()));
    CHECK(value_of(facts, "psd_sites") == 3);
    CHECK(value_of(facts, "psd_events") == 4);
    REQUIRE(facts.events.size() == 3);
    CHECK(facts.events[0].first == "7110");
    CHECK(facts.events[0].second == std::vector<std::string>{"10:094:81643", "12:001:00000"});
    CHECK(facts.events[1].first == "7237");
    CHECK(facts.events[1].second == std::vector<std::string>{"11:070:20783"});
    CHECK(facts.events[2].first == "7600");
    CHECK(facts.events[2].second == std::vector<std::string>{"14:001:00000"});
    // the keys are in print order: the twenty-two counts, as the tool prints them
    CHECK(facts.counts.size() == 22);
    CHECK(facts.counts.front().first == "slrf_site_id_rows");
    CHECK(facts.counts.back().first == "psd_events");
}

TEST_CASE("SINEX epochs: the pivot of the two-digit year, the end of a day, leap years, the unknown start and the open end", "[measmod_registry_facts][behaviour]") {
    const auto gaps_of = [](const std::vector<std::pair<std::string, std::string>>& spans) {
        // one marker, solutions numbered 1, 2, ... with these (start, end) pairs, and nothing else a count needs
        std::string slrf = "+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/ID\n+SOLUTION/EPOCHS\n";
        for (std::size_t i = 0; i < spans.size(); ++i) slrf += epoch_line("1000", "A", static_cast<int>(i) + 1, spans[i].first, spans[i].second);
        slrf += "-SOLUTION/EPOCHS\n";
        return value_of(rf::count(lines_of(slrf), {}, {}), "slrf_gaps_between_solutions");
    };
    // 49 is 2049 and 50 is 1950: solution 1 ends 2049-12-31 23:59:58, solution 2 starts 1950-01-01 -- it starts BEFORE the other ends: no gap (with 49 read as 1949 the first would end before the second starts)
    CHECK(gaps_of({{"40:001:00000", "49:365:86398"}, {"50:001:00000", "00:000:00000"}}) == 0);
    // and the other way: 48 is 2048 and 51 is 1951
    CHECK(gaps_of({{"90:001:00000", "49:001:00000"}, {"49:100:00000", "00:000:00000"}}) == 1);   // ends 2049-01-01, starts 2049-04-10: a gap
    // the end of a day: 86399 s means the start of the next day -- 99:365:86399 is 2000-01-01 00:00:00 and a solution that starts there leaves no gap; 86398 is the second before the end of the day, a gap of one
    CHECK(gaps_of({{"90:001:00000", "99:365:86399"}, {"00:001:00000", "00:000:00000"}}) == 0);
    CHECK(gaps_of({{"90:001:00000", "99:365:86398"}, {"00:001:00000", "00:000:00000"}}) == 1);
    // only an END has the convention: a START at 86399 is the second before the end of its day, so a start there is one second after an end at 86398: a gap of one
    CHECK(gaps_of({{"90:001:00000", "99:365:86398"}, {"99:365:86399", "00:000:00000"}}) == 1);   // ends 23:59:58, starts 23:59:59
    CHECK(gaps_of({{"90:001:00000", "99:365:86399"}, {"99:365:86399", "00:000:00000"}}) == 0);   // ends 2000-01-01 00:00:00, starts 1999-12-31 23:59:59: no gap either (it starts before the end)
    // a day 366 exists in a leap year (2000) and not otherwise: 00:366:86399 is 2001-01-01, where 01:001 starts; 01:365:86399 is 2002-01-01, where 02:001 starts
    CHECK(gaps_of({{"00:001:00000", "00:366:86399"}, {"01:001:00000", "00:000:00000"}}) == 0);
    CHECK(gaps_of({{"01:001:00000", "01:365:86399"}, {"02:001:00000", "00:000:00000"}}) == 0);
    CHECK(gaps_of({{"01:001:00000", "01:365:86398"}, {"02:001:00000", "00:000:00000"}}) == 1);
    // the numbers are those of the ROWS here (1 then 2): the first has no end
    CHECK(gaps_of({{"96:001:00000", "00:000:00000"}, {"90:001:00000", "95:100:00000"}}) == 0);
    // three solutions: each consecutive pair is looked at, an unknown end or an unknown start excuses it
    CHECK(gaps_of({{"90:001:00000", "91:001:00000"}, {"92:001:00000", "93:001:00000"}, {"94:001:00000", "00:000:00000"}}) == 2);
    CHECK(gaps_of({{"90:001:00000", "00:000:00000"}, {"92:001:00000", "93:001:00000"}, {"94:001:00000", "00:000:00000"}}) == 1);
    CHECK(gaps_of({{"90:001:00000", "91:001:00000"}, {"00:000:00000", "93:001:00000"}, {"94:001:00000", "00:000:00000"}}) == 1);
}

TEST_CASE("the order of the solutions is the order of their numbers", "[measmod_registry_facts][behaviour]") {
    // solution 2 is listed first; 1 ends 1995-04-10 and 2 starts 1996-01-01: one gap whichever way they are listed (read in the file's order, 2's end and 1's start would be compared and find none)
    std::string slrf = "+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/ID\n+SOLUTION/EPOCHS\n";
    slrf += epoch_line("1000", "A", 2, "96:001:00000", "00:000:00000");
    slrf += epoch_line("1000", "A", 1, "90:001:00000", "95:100:00000");
    slrf += "-SOLUTION/EPOCHS\n";
    CHECK(value_of(rf::count(lines_of(slrf), {}, {}), "slrf_gaps_between_solutions") == 1);
    // numbers are compared as numbers: 10 comes after 9 (as text it would come before 2)
    std::string numbered = "+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/ID\n+SOLUTION/EPOCHS\n";
    numbered += epoch_line("1000", "A", 10, "96:001:00000", "00:000:00000");
    numbered += epoch_line("1000", "A", 9, "90:001:00000", "95:100:00000");
    numbered += "-SOLUTION/EPOCHS\n";
    CHECK(value_of(rf::count(lines_of(numbered), {}, {}), "slrf_gaps_between_solutions") == 1);
}

TEST_CASE("days_from_civil: every day of the years 0001 to 9999 and of the 800 years before agrees with a calendar walked one day at a time, and the anchors with GNU date", "[measmod_registry_facts][behaviour]") {
    // the oracle: a calendar that knows the lengths of the months and the leap rule and nothing else, counting from 1970-01-01 (day 0) forward to 9999-12-31 and backward to the year -799
    const auto leap = [](std::int64_t y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; };
    const auto length_of = [&](std::int64_t y, int m) {
        static const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return m == 2 && leap(y) ? 29 : days[m - 1];
    };
    std::int64_t wrong = 0;
    std::string first_wrong;
    const auto compare = [&](std::int64_t y, int m, int d, std::int64_t counted) {
        const std::int64_t got = rf::days_from_civil(y, m, d);
        if (got != counted && wrong++ == 0) first_wrong = std::to_string(y) + "-" + std::to_string(m) + "-" + std::to_string(d) + " gave " + std::to_string(got) + ", the walk counted " + std::to_string(counted);
    };
    std::int64_t counted = 0;
    for (std::int64_t y = 1970; y <= 9999; ++y) {
        for (int m = 1; m <= 12; ++m) {
            for (int d = 1; d <= length_of(y, m); ++d) compare(y, m, d, counted++);
        }
    }
    CHECK(counted == 2932897);   // the day after 9999-12-31, which is day 2932896: its proleptic ordinal 3652059 less 719163, that of 1970-01-01
    counted = -1;
    std::int64_t year_one = 0;
    for (std::int64_t y = 1969; y >= -799; --y) {
        for (int m = 12; m >= 1; --m) {
            for (int d = length_of(y, m); d >= 1; --d) {
                compare(y, m, d, counted);
                if (y == 1 && m == 1 && d == 1) year_one = counted;
                --counted;
            }
        }
    }
    CHECK(year_one == -719162);   // 0001-01-01: the ordinal 1 less 719163
    INFO(first_wrong);
    CHECK(wrong == 0);
    // the anchors: `date -u -d YYYY-MM-DD +%s` divided by 86400 (GNU date refuses 9999-12-31: its day is the one after 9999-12-30)
    struct Anchor {
        std::int64_t year;
        int month;
        int day;
        std::int64_t days;
    };
    for (const Anchor& a : std::vector<Anchor>{{1, 1, 1, -719162}, {0, 3, 1, -719468}, {1600, 3, 1, -135080}, {1900, 2, 28, -25509}, {1900, 3, 1, -25508}, {1969, 12, 31, -1}, {1970, 1, 1, 0}, {1970, 3, 1, 59},
                                               {2000, 2, 29, 11016}, {2000, 3, 1, 11017}, {2024, 2, 29, 19782}, {2100, 2, 28, 47540}, {2100, 3, 1, 47541}, {4000, 2, 29, 741501},
                                               {4000, 3, 1, 741502}, {9999, 12, 30, 2932895}, {9999, 12, 31, 2932896}}) {
        INFO(a.year << "-" << a.month << "-" << a.day);
        CHECK(rf::days_from_civil(a.year, a.month, a.day) == a.days);
    }
}

TEST_CASE("snx_epoch: the year, the day and the seconds as datetime and timedelta take them, the day-end convention and the open end", "[measmod_registry_facts][behaviour]") {
    const auto at = [](const std::string& epoch, bool end = false) { return rf::snx_epoch(epoch, end); };
    const auto instant = [](std::int64_t y, int m, int d, std::int64_t seconds) { return std::optional<std::int64_t>(rf::days_from_civil(y, m, d) * 86400 + seconds); };
    // 00:000 is the open end or the unknown start whatever follows it; any other 00 or 000 is a date (the strings are compared, not the numbers)
    CHECK(at("00:000:00000") == std::nullopt);
    CHECK(at("00:000:00000", true) == std::nullopt);
    CHECK(at("00:000:xyz") == std::nullopt);
    CHECK(at("00:001:00000") == instant(2000, 1, 1, 0));
    CHECK(at("0:000:00000") == instant(1999, 12, 31, 0));
    CHECK(at("00:00:00000") == instant(1999, 12, 31, 0));
    CHECK(at("01:000:00000") == instant(2000, 12, 31, 0));
    // the pivot: below 50 is 20yy, from 50 on is 19yy; a negative number is below 50
    CHECK(at("49:001:00000") == instant(2049, 1, 1, 0));
    CHECK(at("50:001:00000") == instant(1950, 1, 1, 0));
    CHECK(at("99:001:00000") == instant(1999, 1, 1, 0));
    CHECK(at("-1:001:00000") == instant(1999, 1, 1, 0));
    // the day of the year counts from 1 and may run past the year or before it; the seconds past a day and the negative seconds count into the days around
    CHECK(at("90:001:00000") == instant(1990, 1, 1, 0));
    CHECK(at("90:032:00000") == instant(1990, 2, 1, 0));
    CHECK(at("92:366:00000") == instant(1992, 12, 31, 0));
    CHECK(at("92:367:00000") == instant(1993, 1, 1, 0));
    CHECK(at("90:001:90000") == instant(1990, 1, 2, 3600));
    CHECK(at("90:001:86399") == instant(1990, 1, 1, 86399));
    CHECK(at("90:001:86400") == instant(1990, 1, 2, 0));
    CHECK(at("90:002:-1") == instant(1990, 1, 1, 86399));
    CHECK(at("90:001:-86400") == instant(1989, 12, 31, 0));
    CHECK(at("90:001:-86401") == instant(1989, 12, 30, 86399));
    CHECK(at("90:000:00000") == instant(1989, 12, 31, 0));
    CHECK(at("90:-1:00000") == instant(1989, 12, 30, 0));
    // the end of a day: the seconds 86399 of an END mean the start of the next day; of a start they are the last second of the day; no other seconds mean anything
    CHECK(at("90:001:86399", true) == instant(1990, 1, 2, 0));
    CHECK(at("90:001:86399", false) == instant(1990, 1, 1, 86399));
    CHECK(at("90:001:86398", true) == instant(1990, 1, 1, 86398));
    CHECK(at("90:365:86399", true) == instant(1991, 1, 1, 0));
    CHECK(at("92:366:86399", true) == instant(1993, 1, 1, 0));
    // a sign, underscores between digits, and the digits of other scripts are taken as int() takes them
    CHECK(at("+90:+001:+00000") == instant(1990, 1, 1, 0));
    CHECK(at("1_0:0_0_1:1_0_0") == instant(2010, 1, 1, 100));
    CHECK(at("9_0:001:0") == instant(1990, 1, 1, 0));
    CHECK(at("\xD9\xA9\xD9\xA0:001:00000") == instant(1990, 1, 1, 0));    // U+0669 U+0660, the Arabic-Indic nine and zero
    CHECK(at("\xD9\xA9_\xD9\xA0:001:00000") == instant(1990, 1, 1, 0));
}

TEST_CASE("snx_epoch: what is not an epoch is refused, naming the fault; so is what no datetime holds, and a sum that would overflow is refused and never wrapped", "[measmod_registry_facts][behaviour]") {
    const auto at = [](const std::string& epoch, bool end = false) { return rf::snx_epoch(epoch, end); };
    const auto instant = [](std::int64_t y, int m, int d, std::int64_t seconds) { return std::optional<std::int64_t>(rf::days_from_civil(y, m, d) * 86400 + seconds); };
    const auto refusal = [](const std::string& epoch, bool end = false) { return message_of([&] { (void)rf::snx_epoch(epoch, end); }); };
    // the shape
    CHECK(refusal("90:001") == "an epoch '90:001' is not YY:DOY:SSSSS");
    CHECK(refusal("90:001:00000:5") == "an epoch '90:001:00000:5' is not YY:DOY:SSSSS");
    CHECK(refusal("") == "an epoch '' is not YY:DOY:SSSSS");
    // the integers
    CHECK(refusal("xx:001:00000") == "the year of an epoch: 'xx' is not an integer");
    CHECK(refusal("90:abc:00000") == "the day of an epoch: 'abc' is not an integer");
    CHECK(refusal("90:001:0z000") == "the seconds of an epoch: '0z000' is not an integer");
    CHECK(refusal(":001:00000") == "the year of an epoch: '' is not an integer");
    CHECK(refusal("90::00000") == "the day of an epoch: '' is not an integer");
    CHECK(refusal("90:001:") == "the seconds of an epoch: '' is not an integer");
    CHECK(refusal("+:001:00000") == "the year of an epoch: '+' is not an integer");
    CHECK(refusal("-:001:00000") == "the year of an epoch: '-' is not an integer");
    CHECK(refusal("--1:001:00000") == "the year of an epoch: '--1' is not an integer");
    CHECK(refusal("1__0:001:00000") == "the year of an epoch: '1__0' is not an integer");
    CHECK(refusal("_10:001:00000") == "the year of an epoch: '_10' is not an integer");
    CHECK(refusal("10_:001:00000") == "the year of an epoch: '10_' is not an integer");
    CHECK(refusal("+_1:001:00000") == "the year of an epoch: '+_1' is not an integer");
    CHECK(refusal("\xD9\xA9x:001:00000") == "the year of an epoch: '\xD9\xA9x' is not an integer");
    CHECK(refusal("90:9223372036854775808:0") == "the day of an epoch: '9223372036854775808' is beyond the range this reads");
    CHECK(refusal("90:-9223372036854775808:0") == "the day of an epoch: '-9223372036854775808' is beyond the range this reads");
    // what datetime holds: the years 1 to 9999 (yy from -1999 to 8099) and, of them, the instants from 0001-01-01 00:00:00 to 9999-12-31 23:59:59
    CHECK(at("-1999:001:00000") == instant(1, 1, 1, 0));
    CHECK(at("8099:365:86399") == instant(9999, 12, 31, 86399));
    CHECK(at("8099:365:86398", true) == instant(9999, 12, 31, 86398));
    CHECK(refusal("-2000:001:00000") == "the year of the epoch '-2000:001:00000' is out of range");
    CHECK(refusal("8100:001:00000") == "the year of the epoch '8100:001:00000' is out of range");
    CHECK(refusal("-1999:000:00000") == "the epoch '-1999:000:00000' is out of range");           // a day before 0001-01-01
    CHECK(refusal("-1999:001:-1") == "the epoch '-1999:001:-1' is out of range");                 // a second before it
    CHECK(refusal("8099:366:00000") == "the epoch '8099:366:00000' is out of range");             // the first day of the year 10000
    CHECK(refusal("8099:365:86400") == "the epoch '8099:365:86400' is out of range");             // a second after 9999-12-31 23:59:59
    CHECK(refusal("8099:365:86399", true) == "the epoch '8099:365:86399' is out of range");       // the end of the last day is the start of the next
    // the days and the seconds are added exactly: those that cancel make a date, those that overflow 64 bits are refused whichever way they go, and a day number that fits and is far from any date is refused too
    CHECK(at("90:-9999999999999:864000000000000000") == instant(1990, 1, 1, 0));            // -10^13 days and 10^13 days of seconds
    CHECK(at("90:10000000000001:-864000000000000000") == instant(1990, 1, 1, 0));
    CHECK(at("90:-9999999999999:864000000000086399") == instant(1990, 1, 1, 86399));
    CHECK(refusal("90:9223372036854775807:432000") == "the epoch '90:9223372036854775807:432000' is out of range");    // 2^63-2 days and 5 days of seconds
    CHECK(refusal("90:-9223372036854775807:-86400") == "the epoch '90:-9223372036854775807:-86400' is out of range");   // -2^63 days and one more
    CHECK(refusal("90:9223372036854775807:0") == "the epoch '90:9223372036854775807:0' is out of range");
    CHECK(refusal("90:-9223372036854775807:0") == "the epoch '90:-9223372036854775807:0' is out of range");
    CHECK(refusal("90:9223372036854775807:86399", true) == "the epoch '90:9223372036854775807:86399' is out of range");
    CHECK(refusal("90:001:9223372036854775807") == "the epoch '90:001:9223372036854775807' is out of range");
    CHECK(refusal("90:001:-9223372036854775807") == "the epoch '90:001:-9223372036854775807' is out of range");
}

TEST_CASE("the solutions of a marker are put in the order of (number, start, end), an unknown epoch being equal only to an unknown one; the gaps are those of that order", "[measmod_registry_facts][behaviour]") {
    const auto gaps = [](const std::vector<std::array<std::string, 3>>& solutions) { return value_of(rf::count(lines_of(one_marker(solutions)), {}, {}), "slrf_gaps_between_solutions"); };
    const std::string open = "00:000:00000";
    // equal numbers: by start, whichever order they are listed in, and the end is not what decides (by ends the order would be the other one and the gap would be missed or invented): A starts 1990 ends 1999,
    // B starts 1991 ends 1992, C (number 2) starts 1995 -- A, B, C has B's end before C's start: one gap
    CHECK(gaps({{"1", "91:001:00000", "92:001:00000"}, {"1", "90:001:00000", "99:001:00000"}, {"2", "95:001:00000", open}}) == 1);
    CHECK(gaps({{"1", "90:001:00000", "99:001:00000"}, {"1", "91:001:00000", "92:001:00000"}, {"2", "95:001:00000", open}}) == 1);
    // equal starts: by end, the earlier first, whichever order they are listed in -- C starts 1992, between the ends 1991 and 1993: B, with the later end, is the last before C, and it does not end before C starts
    CHECK(gaps({{"1", "90:001:00000", "91:001:00000"}, {"1", "90:001:00000", "93:001:00000"}, {"2", "92:001:00000", open}}) == 0);
    CHECK(gaps({{"1", "90:001:00000", "93:001:00000"}, {"1", "90:001:00000", "91:001:00000"}, {"2", "92:001:00000", open}}) == 0);
    // unknown starts are equal to each other, and then the ends decide
    CHECK(gaps({{"1", open, "93:001:00000"}, {"1", open, "91:001:00000"}, {"2", "92:001:00000", open}}) == 0);
    CHECK(gaps({{"1", open, "91:001:00000"}, {"1", open, "93:001:00000"}, {"2", "92:001:00000", open}}) == 0);
    // two solutions alike in everything: their order makes no difference
    CHECK(gaps({{"1", "90:001:00000", "91:001:00000"}, {"1", "90:001:00000", "91:001:00000"}, {"2", "92:001:00000", open}}) == 1);
    CHECK(gaps({{"1", "90:001:00000", open}, {"1", "90:001:00000", open}, {"2", "92:001:00000", open}}) == 0);
    // a start known against a start unknown, or an end known against an end unknown, with the same number (and the same start) cannot be put in order: the Python raised TypeError
    const std::string unordered = "two solutions of one marker have the same number and only one has an epoch";
    CHECK(message_of([&] { (void)gaps({{"1", open, "91:001:00000"}, {"1", "90:001:00000", "91:001:00000"}}); }) == unordered);
    CHECK(message_of([&] { (void)gaps({{"1", "90:001:00000", "91:001:00000"}, {"1", open, "91:001:00000"}}); }) == unordered);
    CHECK(message_of([&] { (void)gaps({{"1", "90:001:00000", open}, {"1", "90:001:00000", "91:001:00000"}}); }) == unordered);
    CHECK(message_of([&] { (void)gaps({{"1", "90:001:00000", "91:001:00000"}, {"1", "90:001:00000", open}}); }) == unordered);
    // with other numbers an unknown against a known is not compared
    CHECK(gaps({{"1", open, "91:001:00000"}, {"2", "90:001:00000", "91:001:00000"}}) == 0);
    // the numbers are integers as int() reads them: -1 comes before +2 though it is listed after (the gap: -1 ends 1995-04-10, +2 starts 1996-01-01); 1_0 is ten, which comes after nine; U+0661 U+0660 is ten too
    CHECK(gaps({{"+2", "96:001:00000", open}, {"-1", "90:001:00000", "95:100:00000"}}) == 1);
    CHECK(gaps({{"1_0", "96:001:00000", open}, {"9", "90:001:00000", "95:100:00000"}}) == 1);
    CHECK(gaps({{"\xD9\xA1\xD9\xA0", "96:001:00000", open}, {"9", "90:001:00000", "95:100:00000"}}) == 1);
    // the numbers at the ends of the 64 bits
    CHECK(gaps({{"9223372036854775807", "96:001:00000", open}, {"9223372036854775806", "90:001:00000", "95:100:00000"}}) == 1);
    CHECK(gaps({{"-9223372036854775806", "96:001:00000", open}, {"-9223372036854775807", "90:001:00000", "95:100:00000"}}) == 1);
    CHECK(message_of([&] { (void)gaps({{"9223372036854775808", "90:001:00000", open}}); }) == "a solution number: '9223372036854775808' is beyond the range this reads");
    CHECK(message_of([&] { (void)gaps({{"-9223372036854775808", "90:001:00000", open}}); }) == "a solution number: '-9223372036854775808' is beyond the range this reads");
    CHECK(message_of([&] { (void)gaps({{"x", "90:001:00000", open}}); }) == "a solution number: 'x' is not an integer");
}

TEST_CASE("a SOLUTION/EPOCHS row of six fields is a row (the mean epoch is not read), of five it is refused; a SITE/ID line that is blank is refused, naming the file", "[measmod_registry_facts][behaviour]") {
    const std::string site = "+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/ID\n";
    const rf::Facts six = rf::count(lines_of(site + "+SOLUTION/EPOCHS\n 1000  A    1 C 90:001:00000 00:000:00000\n-SOLUTION/EPOCHS\n"), {}, {});
    CHECK(value_of(six, "slrf_solutions") == 1);
    CHECK(value_of(six, "slrf_markers_with_solution") == 1);
    CHECK(message_of([&] { (void)rf::count(lines_of(site + "+SOLUTION/EPOCHS\n 1000  A    1 C 90:001:00000\n-SOLUTION/EPOCHS\n"), {}, {}); }) == "a SOLUTION/EPOCHS line has 5 fields, six are wanted");
    CHECK(message_of([&] { (void)rf::count(lines_of(site + "+SOLUTION/EPOCHS\n 1000  A\n-SOLUTION/EPOCHS\n"), {}, {}); }) == "a SOLUTION/EPOCHS line has 2 fields, six are wanted");
    CHECK(message_of([&] { (void)rf::count(lines_of("+SITE/ID\n\n-SITE/ID\n"), {}, {}); }) == "SITE/ID of SLRF2020: a blank line where a record is wanted");
    CHECK(message_of([&] { (void)rf::count({}, lines_of("+SITE/ID\n  \n-SITE/ID\n"), {}); }) == "SITE/ID of the eccentricity file: a blank line where a record is wanted");
}

TEST_CASE("the header's refusals say what is wrong with the line, and the number is the first word between the first 'for' and the next", "[measmod_registry_facts][behaviour]") {
    const auto refusal = [](const std::string& line) { return message_of([&] { (void)rf::count({line}, {}, {}); }); };
    CHECK(refusal("* unique sites") == "the header line '* unique sites' has no 'for'");
    CHECK(refusal("* unique sites for") == "the header line '* unique sites for' has no number after 'for'");
    CHECK(refusal("* unique sites for  ") == "the header line '* unique sites for  ' has no number after 'for'");
    CHECK(refusal("* informations for unique sites") == "the header's count of unique sites: 'mations' is not an integer");
    // nothing between the first 'for' and the second: no number there (the word after the second one is not taken instead)
    CHECK(refusal("* for for 5 unique sites") == "the header line '* for for 5 unique sites' has no number after 'for'");
    CHECK(refusal("* for  for 5 unique sites") == "the header line '* for  for 5 unique sites' has no number after 'for'");
    CHECK(refusal("* for abc unique sites") == "the header's count of unique sites: 'abc' is not an integer");
    // the first word only, the line's end ending the text; a line that begins with the 'for'
    const auto sites_of = [](const std::string& line) { return value_of(rf::count({line}, {}, {}), "slrf_header_sites"); };
    CHECK(sites_of("for 7 unique sites") == 7);
    CHECK(sites_of("* for 7 8 9 unique sites") == 7);
    CHECK(sites_of("unique sites for 7") == 7);
    CHECK(sites_of("* for 7for 8 unique sites") == 7);
}

TEST_CASE("the post-seismic site number is a field of decimal digits, whichever character is not one and whatever the lengths of the fields; four fields are enough", "[measmod_registry_facts][behaviour]") {
    using Events = std::vector<std::pair<std::string, std::vector<std::string>>>;
    const auto events_of = [](const std::string& text) { return rf::count({}, {}, lines_of(text)).events; };
    // exactly four fields, the fourth with two colons
    CHECK(events_of(" 7700 A 60000M001 14:001:00000\n") == Events{{"7700", {"14:001:00000"}}});
    // the first field with a character that is not a digit first, last, in the middle (with the second field shorter than the first), and a first field shorter than the second
    CHECK(events_of(" a110 A x 1:2:3\n").empty());
    CHECK(events_of(" 110a A x 1:2:3\n").empty());
    CHECK(events_of(" 71x0 A x 1:2:3\n").empty());
    CHECK(events_of(" 7 LONGFIELD x 1:2:3\n") == Events{{"7", {"1:2:3"}}});
    // digits of another script are digits (Unicode category Nd; the superscripts that isdigit() also admits are not: the deviation said in the tool's header)
    CHECK(events_of(" \xD9\xA7\xD9\xA1 A x 1:2:3\n") == Events{{"\xD9\xA7\xD9\xA1", {"1:2:3"}}});
    CHECK(events_of(" \xC2\xB2 A x 1:2:3\n").empty());
    // the fourth field must have two colons exactly; three fields are not a record
    CHECK(events_of(" 7 A x 1:2\n").empty());
    CHECK(events_of(" 7 A x 1:2:3:4\n").empty());
    CHECK(events_of(" 7 A x 12\n").empty());
    CHECK(events_of(" 7 A 1:2:3\n").empty());
    CHECK(events_of("\n").empty());
}

TEST_CASE("the header's count: the number after the first 'for' in the first 200 lines, or -1", "[measmod_registry_facts][behaviour]") {
    const auto header_of = [](const std::vector<std::string>& lines) {
        const rf::Facts facts = rf::count(lines, {}, {});
        return std::make_pair(value_of(facts, "slrf_header_sites"), value_of(facts, "pads_added_after_header"));
    };
    CHECK(header_of({"* The archive contains positions and velocities for 184 unique sites."}) == std::make_pair<std::int64_t, std::int64_t>(184, -1));   // (no pads at all: -1)
    CHECK(header_of({"no sentence here"}) == std::make_pair<std::int64_t, std::int64_t>(-1, -1));
    // the first line that holds the words is the one read, and it is read once
    CHECK(header_of({"* unique sites for 7 or so", "* positions for 99 unique sites."}).first == 7);
    // the line must be among the first 200: the 200th line is read, the 201st is not
    std::vector<std::string> lines(199, "*");
    lines.push_back("* for 5 unique sites");
    CHECK(header_of(lines).first == 5);
    lines.insert(lines.begin(), "*");
    CHECK(header_of(lines).first == -1);
    // the text is split at EVERY 'for': the number is the first word of the piece between the first and the second ("information" holds one)
    CHECK(header_of({"* contains for 5 unique sites for"}).first == 5);
    // refused: the words without a 'for', nothing after it, a word that is no integer
    CHECK_THROWS_AS(header_of({"* unique sites"}), rf::BadInput);
    CHECK_THROWS_AS(header_of({"* unique sites for"}), rf::BadInput);
    CHECK_THROWS_AS(header_of({"* informations for unique sites"}), rf::BadInput);     // 'informations' holds a 'for': the pieces are '* in', 'mations ' and ' unique sites', and the word after the first is 'mations'
    // pads: with the sentence's count equal to the pads left after 7329 and 7317 are removed, the two are 'added after the header'; with any other count -1
    const std::string slrf_ok = std::string("* for 1 unique sites\n+SITE/ID\n") + site_line("1000", "A", "10000S001", "10000001") + site_line("7329", "A", "73290S001", "73290001") + site_line("7317", "A", "73170S001", "73170001") + "-SITE/ID\n";
    CHECK(header_of(lines_of(slrf_ok)) == std::make_pair<std::int64_t, std::int64_t>(1, 2));
    const std::string slrf_one = std::string("* for 1 unique sites\n+SITE/ID\n") + site_line("1000", "A", "10000S001", "10000001") + site_line("7329", "A", "73290S001", "73290001") + "-SITE/ID\n";
    CHECK(header_of(lines_of(slrf_one)) == std::make_pair<std::int64_t, std::int64_t>(1, 1));
    const std::string slrf_bad = std::string("* for 2 unique sites\n+SITE/ID\n") + site_line("1000", "A", "10000S001", "10000001") + site_line("7329", "A", "73290S001", "73290001") + "-SITE/ID\n";
    CHECK(header_of(lines_of(slrf_bad)) == std::make_pair<std::int64_t, std::int64_t>(2, -1));
}

TEST_CASE("columns are code points: a letter beyond ASCII before the key columns shifts nothing", "[measmod_registry_facts][behaviour]") {
    // the same two rows in both files but for the last character of the DOMES number: a reading by bytes would take the slice 9:18 one character too early and leave that character out, so the keys would be equal
    const std::string row_slrf = site_line("10\xC3\xA9" "0", "A", "10000S001", "10000001");
    const std::string row_ecc = site_line("10\xC3\xA9" "0", "A", "10000S002", "10000001");
    const std::string slrf = "+SITE/ID\n" + row_slrf + "-SITE/ID\n+SOLUTION/EPOCHS\n" + epoch_line("10\xC3\xA9" "0", "A", 1, "90:001:00000", "00:000:00000") + "-SOLUTION/EPOCHS\n";
    const std::string ecc = "+SITE/ID\n" + row_ecc + "-SITE/ID\n";
    const rf::Facts facts = rf::count(lines_of(slrf), lines_of(ecc), {});
    CHECK(value_of(facts, "pad_point_domes_disagreements") == 1);
    CHECK(value_of(facts, "placed_sods") == 0);
    // with the same DOMES the row is placed: the pad is the four code points '10é0', the point 'A'
    const rf::Facts same = rf::count(lines_of(slrf), lines_of("+SITE/ID\n" + row_slrf + "-SITE/ID\n"), {});
    CHECK(value_of(same, "pad_point_domes_disagreements") == 0);
    CHECK(value_of(same, "placed_sods") == 1);
}

TEST_CASE("blocks: the lines between +NAME and -NAME, without the comment lines that begin with *; nothing outside, and the block that is not there is empty", "[measmod_registry_facts][behaviour]") {
    // a SITE/ID block that begins after text, has comment lines inside, and is closed; a second +SITE/ID after the close is not read (the Python's loop breaks at the first -SITE/ID)
    const std::string slrf = "text\n" + std::string("+SITE/ID\n*a comment\n") + site_line("1000", "A", "10000S001", "10000001") + "*another\n" + site_line("2000", "A", "20000S001", "20000001") + "-SITE/ID\n" + "+SITE/ID\n" +
                             site_line("3000", "A", "30000S001", "30000001") + "-SITE/ID\n";
    const rf::Facts facts = rf::count(lines_of(slrf), {}, {});
    CHECK(value_of(facts, "slrf_site_id_rows") == 2);
    // no block at all
    const rf::Facts none = rf::count(lines_of("nothing\n"), lines_of("nothing\n"), {});
    CHECK(value_of(none, "slrf_site_id_rows") == 0);
    CHECK(value_of(none, "ecc_site_id_rows") == 0);
    CHECK(value_of(none, "slrf_pads") == 0);
    // a block that is never closed runs to the end of the file
    const rf::Facts open = rf::count(lines_of("+SITE/ID\n" + site_line("1000", "A", "10000S001", "10000001")), {}, {});
    CHECK(value_of(open, "slrf_site_id_rows") == 1);
    // a line that begins with +SITE/IDX opens it too (startswith), and so does -SITE/IDX close it
    const rf::Facts prefixed = rf::count(lines_of("+SITE/IDX\n" + site_line("1000", "A", "10000S001", "10000001") + "-SITE/IDX\n" + site_line("2000", "A", "20000S001", "20000001")), {}, {});
    CHECK(value_of(prefixed, "slrf_site_id_rows") == 1);
}

TEST_CASE("the printed text: the counts with the key padded to the longest, the events, the unplaced SODs; --check marks each count that is not the pinned one and gives the verdict", "[measmod_registry_facts][behaviour]") {
    const Tree t(slrf_text(), ecc_text(), psd_text());
    const Result plain = t.run();
    CHECK(plain.code == 0);
    CHECK(plain.err.empty());
    const std::string counts =
        "  slrf_site_id_rows                         6\n"
        "  slrf_distinct_sods                        6\n"
        "  slrf_pads                                 5\n"
        "  slrf_markers                              6\n"
        "  slrf_markers_with_solution                5\n"
        "  slrf_solutions                            9\n"
        "  slrf_markers_with_more_than_one_soln      3\n"
        "  slrf_gaps_between_solutions               1\n"
        "  slrf_header_sites                         3\n"
        "  pads_added_after_header                   2\n"
        "  ecc_site_id_rows                         10\n"
        "  ecc_distinct_sods                         8\n"
        "  ecc_pads                                  6\n"
        "  ecc_identical_duplicate_rows              1\n"
        "  pad_point_domes_disagreements             2\n"
        "  placed_sods                               4\n"
        "  unplaced_sods                             4\n"
        "  unplaced_pad_absent                       1\n"
        "  unplaced_sod_absent_on_known_pad          1\n"
        "  unplaced_marker_without_solution          1\n"
        "  psd_sites                                 3\n"
        "  psd_events                                4\n";
    const std::string tail =
        "  PSD events (site: SINEX epochs): {'7110': ['10:094:81643', '12:001:00000'], '7237': ['11:070:20783'], '7600': ['14:001:00000']}\n"
        "  unplaced SODs on pads SLRF2020 lists: ['10000002', '20000002', '30000001']\n";
    CHECK(plain.out == counts + tail);
    // --check: each count that differs from the pinned number carries "   <- EXPECTED n" (three of the twenty-two happen to be the pinned numbers: pads_added_after_header 2, ecc_identical_duplicate_rows 1,
    // unplaced_marker_without_solution 1), and the verdict counts the others
    const Result checked = t.run({"--check"});
    CHECK(checked.code == 1);
    CHECK(checked.err.empty());
    const std::string marked =
        "  slrf_site_id_rows                         6   <- EXPECTED 483\n"
        "  slrf_distinct_sods                        6   <- EXPECTED 483\n"
        "  slrf_pads                                 5   <- EXPECTED 186\n"
        "  slrf_markers                              6   <- EXPECTED 190\n"
        "  slrf_markers_with_solution                5   <- EXPECTED 189\n"
        "  slrf_solutions                            9   <- EXPECTED 237\n"
        "  slrf_markers_with_more_than_one_soln      3   <- EXPECTED 28\n"
        "  slrf_gaps_between_solutions               1   <- EXPECTED 48\n"
        "  slrf_header_sites                         3   <- EXPECTED 184\n"
        "  pads_added_after_header                   2\n"
        "  ecc_site_id_rows                         10   <- EXPECTED 543\n"
        "  ecc_distinct_sods                         8   <- EXPECTED 542\n"
        "  ecc_pads                                  6   <- EXPECTED 235\n"
        "  ecc_identical_duplicate_rows              1\n"
        "  pad_point_domes_disagreements             2   <- EXPECTED 0\n"
        "  placed_sods                               4   <- EXPECTED 482\n"
        "  unplaced_sods                             4   <- EXPECTED 60\n"
        "  unplaced_pad_absent                       1   <- EXPECTED 57\n"
        "  unplaced_sod_absent_on_known_pad          1   <- EXPECTED 2\n"
        "  unplaced_marker_without_solution          1\n"
        "  psd_sites                                 3   <- EXPECTED 8\n"
        "  psd_events                                4   <- EXPECTED 12\n";
    CHECK(checked.out == marked + tail + "FAILED   19 number(s) differ\n");
    // the pinned numbers themselves
    const auto& pinned = rf::expected();
    REQUIRE(pinned.size() == 22);
    CHECK(pinned.front() == std::make_pair<std::string, std::int64_t>("slrf_site_id_rows", 483));
    CHECK(pinned.back() == std::make_pair<std::string, std::int64_t>("psd_events", 12));
    // the table's order is the order of the print: the i-th line of the plain output begins with the i-th pinned key
    const std::vector<std::string> printed = lines_of(plain.out);
    for (std::size_t i = 0; i < pinned.size(); ++i) {
        INFO(pinned[i].first);
        REQUIRE(i < printed.size());
        CHECK(printed[i].compare(2, pinned[i].first.size(), pinned[i].first) == 0);
    }
}

TEST_CASE("an input that is missing or is not the shape expected is refused, exit 2, naming the file or the fault, and nothing is printed", "[measmod_registry_facts][behaviour]") {
    const auto refused = [](const std::string& slrf, const std::string& ecc, const std::string& psd) {
        const Result r = Tree(slrf, ecc, psd).run();
        CHECK(r.code == 2);
        CHECK(r.out.empty());
        CHECK(r.err.find("measmod_registry_facts: ") == 0);
        CHECK(r.err.find(" (run `tools/bootstrap.sh`)\n") != std::string::npos);
        return r.err;
    };
    // a blank line where a SITE/ID record is wanted, in either file
    CHECK(refused("+SITE/ID\n\n-SITE/ID\n", ecc_text(), psd_text()).find("SITE/ID of SLRF2020: a blank line where a record is wanted") != std::string::npos);
    CHECK(refused(slrf_text(), "+SITE/ID\n   \n-SITE/ID\n", psd_text()).find("SITE/ID of the eccentricity file: a blank line where a record is wanted") != std::string::npos);
    // an epoch row with too few fields, a solution number that is no integer, an epoch that is not YY:DOY:SSSSS or has no integer in it
    CHECK(refused("+SOLUTION/EPOCHS\n 1000  A    1 C 90:001:00000\n-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("a SOLUTION/EPOCHS line has 5 fields, six are wanted") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "90:001:00000", "00:000:00000").replace(12, 1, "x") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("is not an integer") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "90:001", "00:000:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("an epoch '90:001' is not YY:DOY:SSSSS") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "90:001:00000:5", "00:000:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("is not YY:DOY:SSSSS") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "xx:001:00000", "00:000:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("the year of an epoch: 'xx' is not an integer") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "90:abc:00000", "00:000:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("the day of an epoch: 'abc' is not an integer") != std::string::npos);
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "90:001:0z000", "00:000:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).find("the seconds of an epoch: '0z000' is not an integer") != std::string::npos);
    // two solutions of one marker with the same number and only one of them with an unknown start: Python's comparison of None with a datetime raised TypeError
    CHECK(refused("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "00:000:00000", "10:001:00000") + epoch_line("1000", "A", 1, "90:001:00000", "10:001:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text())
                  .find("have the same number and only one has an epoch") != std::string::npos);
    // ... but the same number with the same unknown start is an ordinary tuple comparison (the ends then decide)
    CHECK(Tree("+SOLUTION/EPOCHS\n" + epoch_line("1000", "A", 1, "00:000:00000", "10:001:00000") + epoch_line("1000", "A", 1, "00:000:00000", "20:001:00000") + "-SOLUTION/EPOCHS\n", ecc_text(), psd_text()).run().code == 0);
    // a file that does not exist, a directory in its place, a file that is not UTF-8
    for (const char* relative : {rf::kSlrfRelative, rf::kEccRelative, rf::kPsdRelative}) {
        const Tree t(slrf_text(), ecc_text(), psd_text());
        fs::remove(t.root / relative);
        const Result missing = t.run();
        CHECK(missing.code == 2);
        CHECK(missing.err.find(relative) != std::string::npos);
        CHECK(missing.out.empty());
        fs::create_directories(t.root / relative);
        const Result directory = t.run();
        CHECK(directory.code == 2);
        CHECK(directory.err.find(relative) != std::string::npos);
        fs::remove(t.root / relative);
        write_text(t.root / relative, std::string("\xFF\xFE not utf-8\n"));
        const Result bytes = t.run();
        CHECK(bytes.code == 2);
        CHECK(bytes.err.find(relative) != std::string::npos);
    }
}

TEST_CASE("the command line: --check, --root DIR and --root=DIR, -h; everything else is refused, exit 2", "[measmod_registry_facts][behaviour]") {
    const Tree t(slrf_text(), ecc_text(), psd_text());
    const auto run_args = [](const std::vector<std::string>& args) {
        std::ostringstream out;
        std::ostringstream err;
        const int code = rf::run(args, Streams{out, err});
        return Result{code, out.str(), err.str()};
    };
    const std::string help =
        std::string(kUsage) +
        "\n"
        "The registry's data facts, counted independently of the registry: SPEC-measmod.md MEAS-A-004 / MEAS-A-010 state the numbers of the pinned station files (SLRF2020 release 2026.02.05, the ILRS\n"
        "eccentricity file of 2026-05-27, the ITRF2020 SLR post-seismic event list); this reads the three cached files by column and by whitespace field, and counts.  It reads nothing from modules/.\n"
        "\n"
        "options:\n"
        "  -h, --help   show this help and exit\n"
        "  --root ROOT  the tree whose data/cache holds the files (default: the tree this tool was built from)\n"
        "  --check      exit 1 unless every pinned number matches\n"
        "\n"
        "exit codes: 0 printed (and, with --check, every pinned number matched)   1 a pinned number did not match\n"
        "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";
    for (const char* flag : {"-h", "--help"}) {
        const Result r = run_args({flag});
        CHECK(r.code == 0);
        CHECK(r.out == help);
        CHECK(r.err.empty());
    }
    CHECK(run_args({"--root=" + t.root.string()}).code == 0);
    CHECK(run_args({"--root", t.root.string(), "--check"}).code == 1);
    CHECK(run_args({"-h", "--frobnicate"}).code == 0);
    CHECK(run_args({"--frobnicate", "-h"}).code == 2);
    const Result unknown = run_args({"--frobnicate"});
    CHECK(unknown.code == 2);
    CHECK(unknown.err == std::string(kUsage) + "measmod_registry_facts: error: unrecognized arguments: --frobnicate\n");
    CHECK(unknown.out.empty());
    CHECK(run_args({"stray"}).code == 2);
    CHECK(run_args({"--check=1"}).code == 2);
    const Result bare = run_args({"--root"});
    CHECK(bare.code == 2);
    CHECK(bare.err == std::string(kUsage) + "measmod_registry_facts: error: argument --root: expected one argument\n");
    // an argument that looks like an option is not the value of --root: the refusal says so (a root taken from it would be refused as a missing file, in other words)
    for (const char* option_like : {"--check", "---x", "--other"}) {
        const Result r = run_args({"--root", option_like});
        CHECK(r.code == 2);
        CHECK(r.err == std::string(kUsage) + "measmod_registry_facts: error: argument --root: expected one argument\n");
        CHECK(r.out.empty());
    }
    CHECK(rf::default_settings().root == rf::default_root());
}

TEST_CASE("an error the tool did not anticipate is reported with exit status 70", "[measmod_registry_facts][behaviour]") {
    const Tree t(slrf_text(), ecc_text(), psd_text());
    odl::devtools_testing::ThrowingStream out;
    std::ostringstream err;
    CHECK(rf::run({"--root", t.root.string()}, Streams{out, err}) == 70);
    CHECK(err.str() == "measmod_registry_facts: internal error: boom\n");
}

TEST_CASE("on the real files --check prints, byte for byte, what the independent awk computation predicted before the port existed, and every number is the pinned one", "[measmod_registry_facts][real_tree]") {
    std::ostringstream out;
    std::ostringstream err;
    CHECK(rf::run({"--check"}, Streams{out, err}) == 0);
    CHECK(err.str().empty());
    CHECK(out.str() == kControl);
    // without --check the same, less the verdict
    std::ostringstream plain;
    std::ostringstream plain_err;
    CHECK(rf::run({}, Streams{plain, plain_err}) == 0);
    const std::string control = kControl;
    CHECK(plain.str() == control.substr(0, control.rfind("ok       every pinned number matches")));
}
