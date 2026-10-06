// crd_pass_tests.cpp — SPEC-io-formats.md §8, IOFM-A-033 … IOFM-A-038 (v1.1, 2026-10-06): the CRD pass view that L6 step 4's range builder reads.
//
// The structure rules are `CRD2` §1.4 and §4 (a session is an H4…H8 block under an H2 and an H3; a common header set may be followed by
// several blocks, §4.4). The real file is the pinned monthly LAGEOS-1 normal-point file of the EDC (807 sessions, one H9): its counts were taken
// independently of this reader, by record tag and without regard to case (the file mixes `h8` and `H8`).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <odl/io/crd.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace odl::io;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::WithinAbs;

namespace {

std::string slurp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const char* kH1 = "H1 CRD 2 2026 1 1 2\n";
const char* kH2 = "H2 YARL 7090 5 13 3 ILRS\n";
const char* kH3 = "H3 lageos1 7603901 1155 8820 0 1 1\n";
const char* kH4 = "H4 1 2026 1 1 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n";
const char* kC0 = "C0 0 532.000 new la1 mcp ti1 swm met cac\n";
const char* kMet1 = "20 7676.801 976.70 310.30 12. 0\n";
const char* kNp1 = "11 7676.800587100000 0.051212898595 new 2 120.0 1 49.0 0.000 0.000 na 0.17 0 na\n";
const char* kNp2 = "11 7751.000585900000 0.050703827374 new 2 120.0 14 34.0 -0.251 -0.010 na 2.33 0 na\n";
const char* kMet2 = "20 7751.001 976.70 310.20 12. 0\n";
const char* kH8 = "H8\n";
const char* kH9 = "H9\n";

std::string block(const std::string& extra_before_data = "", const std::string& data = "") {
    return std::string(kH4) + kC0 + extra_before_data + (data.empty() ? std::string(kMet1) + kNp1 + kMet2 + kNp2 : data) + kH8;
}

}  // namespace

TEST_CASE("IOFM-A-033  read_crd_passes on hand-built multi-session files in both of CRD2 section 4.4's orderings: the passes' stations, targets, "
          "sessions, configurations, normal points and meteorology are the fixture's, in file order, with the H2, H3 and C0 in force",
          "[io][crd][passes]") {
    // 4.4.2: three complete H1 H2 H3 H4 … H8 sets
    std::string three;
    for (int k = 0; k < 3; ++k) {
        three += kH1;
        three += kH2;
        three += kH3;
        three += block();
    }
    three += kH9;
    auto p = read_crd_passes(three);
    REQUIRE(p.has_value());
    REQUIRE(p->size() == 3);
    for (const CrdPass& q : *p) {
        CHECK(q.station.name == "YARL");
        CHECK((q.station.system_id == 7090 && q.station.system_number == 5 && q.station.occupancy == 13));
        CHECK(q.target.ilrs_id == 7603901);
        CHECK(q.session.start_hour == 2);
        CHECK(q.session.remaining_fields.size() == 4);                 // system delay applied, spacecraft delay, range type, quality
        CHECK(q.session.remaining_fields[0] == "1");
        CHECK(q.session.remaining_fields[2] == "2");
        REQUIRE(q.configs.size() == 1);
        CHECK(q.configs[0].config_id == "new");
        CHECK(q.configs[0].wavelength_nm == 532.0);
        REQUIRE(q.ranges.size() == 2);
        CHECK(q.ranges[0].kind == CrdRecordKind::NormalPointRange);
        CHECK(q.ranges[0].seconds_of_day == 7676.8005871);
        CHECK(q.ranges[0].time_of_flight_s == 0.051212898595);
        CHECK(q.ranges[0].epoch_event == 2);
        CHECK(q.ranges[1].seconds_of_day == 7751.0005859);
        REQUIRE(q.meteorology.size() == 2);
        CHECK(q.meteorology[1].seconds_of_day == 7751.001);
        CHECK(q.time_scale == odl::time::TimeScale::UTC);
    }

    // 4.4.1 (preferred): H1 H2 C0 40 H3 H4 … H8 H3' H4 … H8 H9 — the C0 and the calibration record are HEADER-level (before the first H4) and every block
    // inherits the C0; the second block has its own H3 (another target); the H2 is in force for both
    const std::string kH3b = "H3 lageos2 9207002 1156 22195 0 1 1\n";
    std::string shared = std::string(kH1) + kH2 + kC0 + "40 4957.150591537356 0 new 1652 1639 150.4245 96314.6 9.0 15.3 na na na 2 2 0 3 na\n" + kH3 +
                         kH4 + kMet1 + kNp1 + kH8 + kH3b + kH4 + kMet2 + kNp2 + kH8 + kH9;
    auto s = read_crd_passes(shared);
    REQUIRE(s.has_value());
    REQUIRE(s->size() == 2);
    CHECK((*s)[0].target.ilrs_id == 7603901);
    CHECK((*s)[1].target.ilrs_id == 9207002);
    for (const CrdPass& q : *s) {
        CHECK(q.station.name == "YARL");
        REQUIRE(q.configs.size() == 1);                                // inherited from the header level
        CHECK(q.configs[0].config_id == "new");
        REQUIRE(q.ranges.size() == 1);
    }
    CHECK((*s)[0].ranges[0].seconds_of_day == 7676.8005871);
    CHECK((*s)[1].ranges[0].seconds_of_day == 7751.0005859);
    // a block's OWN C0 is not inherited by the next one
    std::string own = std::string(kH1) + kH2 + kH3 + kH4 + kC0 + kNp1 + kMet1 + kH8 + kH4 + "C0 0 1064.000 old la2 mcp ti1 swm met cac\n" + kNp2 + kMet2 + kH8 + kH9;
    auto o = read_crd_passes(own);
    REQUIRE(o.has_value());
    REQUIRE(o->size() == 2);
    REQUIRE((*o)[0].configs.size() == 1);
    REQUIRE((*o)[1].configs.size() == 1);
    CHECK((*o)[0].configs[0].config_id == "new");
    CHECK((*o)[1].configs[0].config_id == "old");
    // the other records of a block are kept opaque, in file order
    std::string others = std::string(kH1) + kH2 + kH3 + kH4 + "H5 1 25 122900 SGF 08641\n" + kC0 + "C1 0 la1 Nd:Yag 1064.00 5.00 100.00 150.0 6.00 1\n" + kMet1 + kNp1 +
                         "50 new 49.1 0.300\n" + kH8 + kH9;
    auto ot = read_crd_passes(others);
    REQUIRE(ot.has_value());
    REQUIRE((*ot)[0].other.size() == 3);
    CHECK((*ot)[0].other[0].kind == CrdRecordKind::Prediction);
    CHECK((*ot)[0].other[1].kind == CrdRecordKind::Config1);
    CHECK((*ot)[0].other[2].kind == CrdRecordKind::SessionStatistics);
    // tags are case-insensitive, as in the real file
    std::string lower = "h1 CRD 2 2026 1 1 2\nh2 YARL 7090 5 13 3 ILRS\nh3 lageos1 7603901 1155 8820 0 1 1\nh4 1 2026 1 1 2 7 49 2026 1 1 2 25 20 0 0 0 0 1 0 2 0\n"
                        "c0 0 532.000 new la1\n" + std::string(kNp1) + "h8\nH9\n";
    auto lc = read_crd_passes(lower);
    REQUIRE(lc.has_value());
    CHECK(lc->size() == 1);
}

TEST_CASE("IOFM-A-034  real data: the pinned monthly LAGEOS-1 file gives 807 passes, 4 628 normal points and 4 317 meteorology records, every "
          "pass with one C0 and one H4; the first pass is Yarragadee's; the flattened passes agree with the flat read_crd",
          "[io][crd][passes][real]") {
    const std::string text = slurp(ODL_LAGEOS1_NP_FILE);
    auto p = read_crd_passes(text);
    if (!p) FAIL("the real file did not read as passes: " << p.error().id << " " << p.error().message);
    CHECK(p->size() == 807);
    std::size_t nps = 0, mets = 0, no_np = 0;
    for (const CrdPass& q : *p) {
        CHECK(q.configs.size() == 1);
        CHECK(q.station.epoch_time_scale >= 3);
        nps += q.ranges.size();
        mets += q.meteorology.size();
        if (q.ranges.empty()) ++no_np;
        for (const auto& r : q.ranges) CHECK(r.kind == CrdRecordKind::NormalPointRange);
    }
    CHECK(nps == 4628);
    CHECK(mets == 4317);
    CHECK(no_np == 0);
    const CrdPass& first = p->front();
    CHECK(first.station.name == "YARL");
    CHECK((first.station.system_id == 7090 && first.station.system_number == 5 && first.station.occupancy == 13));
    CHECK(first.station.epoch_time_scale == 3);
    CHECK(first.target.name == "lageos1");
    CHECK(first.target.ilrs_id == 7603901);
    CHECK((first.session.start_year == 2026 && first.session.start_month == 1 && first.session.start_day == 1));
    CHECK((first.session.start_hour == 2 && first.session.start_minute == 7 && first.session.start_second == 49));
    REQUIRE(first.session.remaining_fields.size() >= 3);
    CHECK(first.session.remaining_fields[0] == "1");                    // the station's system delay is applied
    CHECK(first.session.remaining_fields[2] == "2");                    // two-way
    REQUIRE_FALSE(first.ranges.empty());
    CHECK(first.ranges[0].seconds_of_day == 7676.8005871);
    CHECK(first.ranges[0].time_of_flight_s == 0.051212898595);
    CHECK(first.ranges[0].config_id == "new");
    CHECK(first.ranges[0].epoch_event == 2);                            // the real file tags the epoch with the TRANSMIT time

    // the view is an indexing of the file, not a second reading: the flat reader finds the same sessions and the same normal points in the same order
    auto flat = read_crd(text);
    REQUIRE(flat.has_value());
    CHECK(flat->sessions.size() == p->size());
    CHECK(flat->stations.size() == p->size());
    std::vector<CrdRangeRecord> joined;
    std::vector<CrdSessionHeader> sessions;
    for (const CrdPass& q : *p) {
        joined.insert(joined.end(), q.ranges.begin(), q.ranges.end());
        sessions.push_back(q.session);
    }
    CHECK(joined == flat->ranges);
    CHECK(sessions == flat->sessions);
}

TEST_CASE("IOFM-A-035  CrdPass::wavelength_nm: the real first pass's configuration 'new' is 532.000 nm; two C0 records select by their own "
          "id; an id with no C0 refuses IOFM-F-014 naming it and the ids held",
          "[io][crd][passes]") {
    auto real = read_crd_passes(slurp(ODL_LAGEOS1_NP_FILE));
    REQUIRE(real.has_value());
    auto w = real->front().wavelength_nm("new");
    REQUIRE(w.has_value());
    CHECK(*w == 532.0);
    auto none = real->front().wavelength_nm("zzz");
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().id == "IOFM-F-014");
    CHECK_THAT(none.error().message, ContainsSubstring("'zzz'"));
    CHECK_THAT(none.error().message, ContainsSubstring("'new'"));

    std::string two = std::string(kH1) + kH2 + kH3 + kH4 + kC0 + "C0 0 1064.000 old la2 mcp ti1 swm met cac\n" + kMet1 + kNp1 + kH8 + kH9;
    auto t = read_crd_passes(two);
    REQUIRE(t.has_value());
    CHECK(*t->front().wavelength_nm("new") == 532.0);
    CHECK(*t->front().wavelength_nm("old") == 1064.0);
    auto bad = t->front().wavelength_nm("NEW");                          // ids are case-sensitive: "new" is not "NEW"
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "IOFM-F-014");
}

TEST_CASE("IOFM-A-036  the H2 time-scale resolution at parse time: codes 3, 4 and 7 resolve to UTC; 0, 1, 2, 5, 6, 8, 9, 10 and 11 each refuse "
          "IOFM-F-015 from read_crd_passes, naming the code; the flat read_crd still reads code 5",
          "[io][crd][passes]") {
    for (int code : {3, 4, 7}) {
        std::string t = std::string(kH1) + "H2 YARL 7090 5 13 " + std::to_string(code) + " ILRS\n" + kH3 + block() + kH9;
        auto p = read_crd_passes(t);
        INFO("code " << code);
        REQUIRE(p.has_value());
        CHECK(p->front().time_scale == odl::time::TimeScale::UTC);
        CHECK(p->front().station.epoch_time_scale == code);
    }
    for (int code : {0, 1, 2, 5, 6, 8, 9, 10, 11}) {
        std::string t = std::string(kH1) + "H2 YARL 7090 5 13 " + std::to_string(code) + " ILRS\n" + kH3 + block() + kH9;
        auto p = read_crd_passes(t);
        INFO("code " << code);
        REQUIRE_FALSE(p.has_value());
        CHECK(p.error().id == "IOFM-F-015");
        CHECK_THAT(p.error().message, ContainsSubstring(std::to_string(code)));
        CHECK_THAT(p.error().message, ContainsSubstring("UTC(BIPM)"));
    }
    // the flat reader is unchanged and lenient: it reads a code of 5 as the integer it is
    std::string five = std::string(kH1) + "H2 YARL 7090 5 13 5 ILRS\n" + kH3 + block() + kH9;
    auto flat = read_crd(five);
    REQUIRE(flat.has_value());
    CHECK(flat->stations.front().epoch_time_scale == 5);
}

TEST_CASE("IOFM-A-037  record 20 typed: the real file's '20  7676.801  976.70 310.30  12. 0' is 7676.801 s, 976.70 mbar, 310.30 K, 12 per cent, "
          "origin 0; a record with a missing or non-numeric field refuses",
          "[io][crd][passes][real]") {
    auto real = read_crd_passes(slurp(ODL_LAGEOS1_NP_FILE));
    REQUIRE(real.has_value());
    REQUIRE_FALSE(real->front().meteorology.empty());
    const CrdMeteorology& m = real->front().meteorology.front();
    CHECK(m.seconds_of_day == 7676.801);
    CHECK(m.pressure_mbar == 976.70);
    CHECK(m.temperature_k == 310.30);
    CHECK(m.relative_humidity_percent == 12.0);
    CHECK(m.origin == 0);
    // the record is ~0.4 ms AFTER the normal point it accompanies (7676.8005871 s): the consumer's job (MEAS-R-035), but the fact is here
    CHECK_THAT(m.seconds_of_day - real->front().ranges.front().seconds_of_day, WithinAbs(0.0004129, 1e-9));
    std::string short_met = std::string(kH1) + kH2 + kH3 + kH4 + kC0 + "20 7676.801 976.70 310.30\n" + kNp1 + kH8 + kH9;
    auto a = read_crd_passes(short_met);
    REQUIRE_FALSE(a.has_value());
    CHECK(a.error().id == "IOFM-F-001");
    std::string nonnum = std::string(kH1) + kH2 + kH3 + kH4 + kC0 + "20 7676.801 976.70 hot 12. 0\n" + kNp1 + kH8 + kH9;
    auto b = read_crd_passes(nonnum);
    REQUIRE_FALSE(b.has_value());
    CHECK(b.error().id == "IOFM-F-001");
}

TEST_CASE("IOFM-A-038  the structure refusals, each alone and each naming its line: a range record before any H4, an H4 with no H3, an H4 before the "
          "previous H8, a block with no H8, two C0 of one id, an H8 with no block; the real file and a header-level calibration record are accepted",
          "[io][crd][passes]") {
    auto expect = [](const std::string& text, const char* what, const char* contains) {
        INFO(what);
        auto p = read_crd_passes(text);
        REQUIRE_FALSE(p.has_value());
        CHECK(p.error().id == "IOFM-F-013");
        CHECK_THAT(p.error().message, ContainsSubstring("CRD line"));
        CHECK_THAT(p.error().message, ContainsSubstring(contains));
    };
    expect(std::string(kH1) + kH2 + kH3 + kNp1 + kH4 + kC0 + kNp2 + kH8, "a normal point before any H4", "outside an H4");
    expect(std::string(kH1) + kH2 + kH3 + kMet1 + kH4 + kC0 + kNp2 + kH8, "a meteorological record before any H4", "outside an H4");
    expect(std::string(kH1) + kH2 + kH4 + kC0 + kNp1 + kH8, "an H4 with no H3", "no H3 in force");
    expect(std::string(kH1) + kH4 + kC0 + kNp1 + kH8, "an H4 with no H2", "no H2 in force");
    expect(std::string(kH1) + kH2 + kH3 + kH4 + kC0 + kNp1 + kH4 + kC0 + kNp2 + kH8, "an H4 before the previous H8", "before the previous block's H8");
    expect(std::string(kH1) + kH2 + kH3 + kH4 + kC0 + kNp1, "no H8 at the end", "not closed by an H8 before the end");
    expect(std::string(kH1) + kH2 + kH3 + kH4 + kC0 + "C0 0 1064.000 new la2\n" + kNp1 + kH8, "two C0 of one id", "second C0");
    expect(std::string(kH1) + kH2 + kH3 + kH8, "an H8 with no block", "no H4 block open");
    expect(std::string(kH1) + kH2 + kH3 + kH4 + kC0 + kNp1 + kH9, "an H9 inside a block", "not closed by an H8");
    // accepted: a calibration record at the header level (CRD2 section 4's examples put "40" before H3 and H4), blank lines, a comment
    const std::string ok = std::string(kH1) + kH2 + kC0 + "40 4957.150591537356 0 new 1652 1639 150.4245 96314.6 9.0 15.3 na na na 2 2 0 3 na\n" + kH3 +
                           "\n00 a comment\n" + kH4 + kMet1 + kNp1 + kH8 + kH9;
    auto p = read_crd_passes(ok);
    REQUIRE(p.has_value());
    CHECK(p->size() == 1);
    // the real file is accepted (so none of these rules fires on it): see IOFM-A-034
    CHECK(read_crd_passes(slurp(ODL_LAGEOS1_NP_FILE)).has_value());
}
