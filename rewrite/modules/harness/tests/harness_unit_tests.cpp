// harness_unit_tests.cpp — L8 step 1 (plan/subplan_L8/L8-1.md, registered 2026-11-27, before this file existed): the SP3 reader's refusals, the SINEX resolver against the pinned release-2446 file, and the statistics against closed forms.
#include "harness_fixture.hpp"

#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>

using namespace odl::harness;
using odl::Mat3;
using odl::Vec3;

namespace {

/// The expected PRN -> SVN -> block table of 2023-01-22 and -23, copied from `awk` over the pinned SINEX (PROVENANCE.md 42.1): independent of the parser under test.
struct Triple { const char* prn; const char* svn; const char* block; };
constexpr std::array<Triple, 31> kExpected{{
    {"G01", "G063", "GPS-IIF"},   {"G02", "G061", "GPS-IIR-B"}, {"G03", "G069", "GPS-IIF"},   {"G04", "G074", "GPS-IIIA"},  {"G05", "G050", "GPS-IIR-M"},
    {"G06", "G067", "GPS-IIF"},   {"G07", "G048", "GPS-IIR-M"}, {"G08", "G072", "GPS-IIF"},   {"G09", "G068", "GPS-IIF"},   {"G10", "G073", "GPS-IIF"},
    {"G11", "G078", "GPS-IIIA"},  {"G12", "G058", "GPS-IIR-M"}, {"G13", "G043", "GPS-IIR-A"}, {"G14", "G077", "GPS-IIIA"},  {"G15", "G055", "GPS-IIR-M"},
    {"G16", "G056", "GPS-IIR-A"}, {"G17", "G053", "GPS-IIR-M"}, {"G18", "G075", "GPS-IIIA"},  {"G19", "G059", "GPS-IIR-B"}, {"G20", "G051", "GPS-IIR-A"},
    {"G21", "G045", "GPS-IIR-A"}, {"G22", "G041", "GPS-IIR-A"}, {"G23", "G076", "GPS-IIIA"},  {"G24", "G065", "GPS-IIF"},   {"G25", "G062", "GPS-IIF"},
    {"G26", "G071", "GPS-IIF"},   {"G27", "G066", "GPS-IIF"},   {"G29", "G057", "GPS-IIR-M"}, {"G30", "G064", "GPS-IIF"},   {"G31", "G052", "GPS-IIR-M"},
    {"G32", "G070", "GPS-IIF"}}};

/// The text of an SP3 file with the `which`-th (1-based) record line of `satellite` carrying a bad x.
std::string with_bad_position(std::string text, const std::string& satellite, int which) {
    const std::string key = "\nP" + satellite;
    std::size_t pos = 0;
    for (int i = 0; i < which; ++i) {
        pos = text.find(key, pos + 1);
        REQUIRE(pos != std::string::npos);
    }
    text.replace(pos + 5, 14, " 999999.999999");
    return text;
}

std::string without_epoch_line(std::string text, int which) {
    std::size_t pos = 0;
    for (int i = 0; i < which; ++i) {
        pos = text.find("\n*  ", pos + 1);
        REQUIRE(pos != std::string::npos);
    }
    const std::size_t end = text.find('\n', pos + 1);
    text.erase(pos, end - pos);
    return text;
}

}  // namespace

TEST_CASE("HARN-A-001 the SP3 reader: positions only decide, and refusals name the epoch", "[harness][sp3]") {
    const std::string t22 = hfx::slurp(ODL_H_SP3_022), t23 = hfx::slurp(ODL_H_SP3_023);
    auto arc = read_sp3_arc(hfx::world(), t22, "G01");
    REQUIRE(arc.has_value());
    CHECK(arc->epochs.size() == 96);
    CHECK(arc->p_itrs_m.size() == 96);
    CHECK(arc->clock_absent_records == 0);
    // G27 on -023: two records whose CLOCK is 999999.999999 and whose positions are valid: accepted, with both counted
    auto g27 = read_sp3_arc(hfx::world(), t23, "G27");
    REQUIRE(g27.has_value());
    CHECK(g27->epochs.size() == 96);
    CHECK(g27->clock_absent_records == 2);
    std::printf("  G27 on -023: %zu epochs, %zu clock-absent records accepted\n", g27->epochs.size(), g27->clock_absent_records);
    // the refusals
    auto bad = read_sp3_arc(hfx::world(), with_bad_position(t22, "G01", 20), "G01");
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "EST-F-901");
    CHECK(bad.error().message.find("epoch 19") != std::string::npos);
    auto gap = read_sp3_arc(hfx::world(), without_epoch_line(t22, 30), "G01");
    REQUIRE_FALSE(gap.has_value());
    CHECK(gap.error().id == "EST-F-901");
    auto absent = read_sp3_arc(hfx::world(), t22, "G28");
    REQUIRE_FALSE(absent.has_value());
    CHECK(absent.error().id == "EST-F-901");
    auto garbage = read_sp3_arc(hfx::world(), "not an sp3 file\n", "G01");
    REQUIRE_FALSE(garbage.has_value());
    // the satellites of the header
    auto prns = list_sp3_prns(t22);
    REQUIRE(prns.has_value());
    CHECK(prns->size() == 31);
    CHECK(std::is_sorted(prns->begin(), prns->end()));
    CHECK(std::find(prns->begin(), prns->end(), "G28") == prns->end());
    CHECK(prns->front() == "G01");
    CHECK(prns->back() == "G32");
}

TEST_CASE("HARN-A-002 the SINEX resolver against the pinned release-2446 file", "[harness][sinex]") {
    const std::string text = hfx::slurp(ODL_H_SINEX);
    auto meta = IgsSatelliteMetadata::parse(text);
    REQUIRE(meta.has_value());
    CHECK(meta->prn_rows() > 0);
    CHECK(meta->identifier_rows() > 0);
    for (const SinexEpoch day : {SinexEpoch{2023, 22, 0}, SinexEpoch{2023, 23, 0}, SinexEpoch{2023, 22, 86399}, SinexEpoch{2023, 23, 86399}}) {
        std::map<std::string, int> per_block;
        for (const Triple& t : kExpected) {
            auto e = meta->resolve_prn(t.prn, day);
            REQUIRE(e.has_value());
            CHECK(e->svn == t.svn);
            CHECK(e->block == t.block);
            ++per_block[e->block];
        }
        CHECK(per_block["GPS-IIF"] == 12);
        CHECK(per_block["GPS-IIR-B"] == 2);
        CHECK(per_block["GPS-IIR-A"] == 5);
        CHECK(per_block["GPS-IIR-M"] == 7);
        CHECK(per_block["GPS-IIIA"] == 5);
        // G28 has no interval on these days: refused, not guessed
        auto g28 = meta->resolve_prn("G28", day);
        REQUIRE_FALSE(g28.has_value());
        CHECK(g28.error().id == "HARN-F-011");
    }
    // EST-Q-012: G01 = G063 = GPS-IIF and G05 = G050 = GPS-IIR-M cover both days
    auto g01 = meta->resolve_prn("G01", SinexEpoch{2023, 22, 0});
    auto g05 = meta->resolve_prn("G05", SinexEpoch{2023, 23, 0});
    REQUIRE(g01.has_value());
    REQUIRE(g05.has_value());
    CHECK((g01->svn == "G063" && g01->block == "GPS-IIF"));
    CHECK((g05->svn == "G050" && g05->block == "GPS-IIR-M"));
    CHECK((g01->valid_from.year == 2011 && g01->valid_from.day_of_year == 197));
    CHECK((g01->valid_to.year == 2024 && g01->valid_to.day_of_year == 103));
    // before an interval: refused, or another satellite's, never G063's
    auto before_gps = meta->resolve_prn("G01", SinexEpoch{1970, 1, 0});
    REQUIRE_FALSE(before_gps.has_value());
    CHECK(before_gps.error().id == "HARN-F-011");
    auto early = meta->resolve_prn("G01", SinexEpoch{2010, 1, 0});
    if (early.has_value()) CHECK(early->svn != "G063");
    auto late = meta->resolve_prn("G01", SinexEpoch{2024, 103, 0});
    if (late.has_value()) CHECK(late->svn != "G063");
    // a calendar date becomes the SINEX day of year
    odl::time::Calendar c;
    c.year = 2023; c.month = 1; c.day = 22; c.hour = 23; c.minute = 59; c.second = 59.0;
    const SinexEpoch e = SinexEpoch::from_calendar(c);
    CHECK((e.year == 2023 && e.day_of_year == 22 && e.seconds == 86399));
    c.year = 2024; c.month = 3; c.day = 1; c.hour = 0; c.minute = 0; c.second = 0.0;
    CHECK(SinexEpoch::from_calendar(c).day_of_year == 61);          // 2024 is a leap year: 31 + 29 + 1
    // refusals of the parser
    CHECK_FALSE(IgsSatelliteMetadata::parse("nothing of a SINEX\n").has_value());
}

TEST_CASE("HARN-A-003 the statistics against closed forms", "[harness][statistics]") {
    using Catch::Matchers::WithinRel;
    // block summary: {1, 2, 3, 4}: mean 2.5, Bessel sd sqrt(5/3)
    auto b = compute_block_summary("A", {1.0, 2.0, 3.0, 4.0});
    REQUIRE(b.has_value());
    CHECK(b->n == 4);
    CHECK(b->mean == 2.5);
    CHECK_THAT(b->sd, WithinRel(std::sqrt(5.0 / 3.0), 1e-15));
    CHECK_FALSE(compute_block_summary("one", {1.0}).has_value());
    CHECK_FALSE(compute_block_summary("none", {}).has_value());
    CHECK_FALSE(compute_block_summary("nan", {1.0, std::nan("")}).has_value());
    // separations: equal n = 3 and equal sd = 1: d_pooled = |dm| / 1 = 1, t_sem = 1 / sqrt(2/3); t_sem / d_pooled = sqrt(n/2)
    auto a3 = compute_block_summary("A", {1.0, 2.0, 3.0});
    auto c3 = compute_block_summary("C", {2.0, 3.0, 4.0});
    REQUIRE(a3.has_value());
    REQUIRE(c3.has_value());
    auto sep = compute_block_separation(*a3, *c3);
    REQUIRE(sep.has_value());
    CHECK_THAT(sep->d_pooled, WithinRel(1.0, 1e-15));
    CHECK_THAT(sep->t_sem, WithinRel(std::sqrt(1.5), 1e-15));
    CHECK_THAT(sep->t_sem / sep->d_pooled, WithinRel(std::sqrt(3.0 / 2.0), 1e-15));
    // unequal n, hand case: A = {0, 2} (n 2, m 1, s sqrt 2), C = {10, 12, 14} (n 3, m 12, s 2)
    auto a2 = compute_block_summary("A", {0.0, 2.0});
    auto c3b = compute_block_summary("C", {10.0, 12.0, 14.0});
    REQUIRE(a2.has_value());
    REQUIRE(c3b.has_value());
    auto sep2 = compute_block_separation(*a2, *c3b);
    REQUIRE(sep2.has_value());
    CHECK_THAT(sep2->d_pooled, WithinRel(11.0 / std::sqrt((1.0 * 2.0 + 2.0 * 4.0) / 3.0), 1e-14));
    CHECK_THAT(sep2->t_sem, WithinRel(11.0 / std::sqrt(2.0 / 2.0 + 4.0 / 3.0), 1e-14));
    // refusals: n < 2 and a zero denominator
    BlockSummary single;
    single.n = 1;
    CHECK_FALSE(compute_block_separation(single, *c3).has_value());
    auto flat_a = compute_block_summary("A", {1.0, 1.0});
    auto flat_b = compute_block_summary("B", {2.0, 2.0});
    REQUIRE(flat_a.has_value());
    REQUIRE(flat_b.has_value());
    CHECK_FALSE(compute_block_separation(*flat_a, *flat_b).has_value());
    // medians
    auto odd = compute_median({3.0, 1.0, 2.0});
    auto even = compute_median({4.0, 1.0, 3.0, 2.0});
    REQUIRE(odd.has_value());
    REQUIRE(even.has_value());
    CHECK(*odd == 2.0);
    CHECK(*even == 2.5);
    CHECK_FALSE(compute_median({}).has_value());
    CHECK(*compute_median({7.0}) == 7.0);
    // the constellation
    auto cs = compute_constellation_summary({0.04, 0.02, 0.03, 0.10}, {true, true, true, false});
    REQUIRE(cs.has_value());
    CHECK(cs->count == 4);
    CHECK(cs->eclipse_free_count == 3);
    CHECK_THAT(cs->median_sn_m, WithinRel(0.035, 1e-14));
    CHECK(cs->max_sn_m == 0.10);
    CHECK_FALSE(compute_constellation_summary({}, {}).has_value());
    CHECK_FALSE(compute_constellation_summary({1.0}, {true, false}).has_value());
    // the orbit residual: two identical epochs, state r = (7e6, 0, 0), v = (0, 7500, 0): radial = x, along = y, cross = z; identity rotation; residual (1, 2, 3) m
    EpochResidual e;
    e.q_minus_p_itrs_m = Vec3{1.0, 2.0, 3.0};
    e.itrs_to_gcrs = Mat3::identity();
    e.position_gcrs_m = Vec3{7.0e6, 0.0, 0.0};
    e.velocity_gcrs_m = Vec3{0.0, 7500.0, 0.0};
    auto s = compute_orbit_residuals({e, e}, 1);
    REQUIRE(s.has_value());
    CHECK_THAT(s->s_n_m, WithinRel(std::sqrt(14.0), 1e-15));
    CHECK_THAT(s->s_n_minus_p_m, WithinRel(std::sqrt(28.0), 1e-15));       // sqrt(sum / (N - p)) = sqrt(28 / 1)
    CHECK_THAT(s->s_n_over_sqrt3_m, WithinRel(std::sqrt(14.0 / 3.0), 1e-15));
    CHECK_THAT(s->rms_radial_m, WithinRel(1.0, 1e-15));
    CHECK_THAT(s->rms_along_m, WithinRel(2.0, 1e-15));
    CHECK_THAT(s->rms_cross_m, WithinRel(3.0, 1e-15));
    CHECK_THAT(s->max_epoch_m, WithinRel(std::sqrt(14.0), 1e-15));
    // refusals: no epochs, parameters >= N, a zero position, a zero angular momentum
    CHECK_FALSE(compute_orbit_residuals({}, 0).has_value());
    CHECK_FALSE(compute_orbit_residuals({e, e}, 2).has_value());
    EpochResidual zero_pos = e;
    zero_pos.position_gcrs_m = Vec3{0.0, 0.0, 0.0};
    CHECK_FALSE(compute_orbit_residuals({zero_pos, zero_pos, zero_pos}, 1).has_value());
    EpochResidual radial_v = e;
    radial_v.velocity_gcrs_m = Vec3{7500.0, 0.0, 0.0};
    CHECK_FALSE(compute_orbit_residuals({radial_v, radial_v, radial_v}, 1).has_value());
}
