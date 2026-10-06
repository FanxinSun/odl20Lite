// truncation_tests.cpp — SPEC-forcemodel.md §3.4 and FMOD-A-020, -021: the truncation criterion as a function, and its table at L4's four points
// for the two registered sets, with the orbit maximum beside the RMS and the predictions written before any measurement.
#include "fixture.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

using namespace odl;

namespace {

/// A registry and a ForceSet that cannot be moved.
struct Set {
    dyn::ParameterRegistry reg;
    dyn::ForceSet fs{reg};
};

std::vector<dyn::Contribution> contributions(Set& s, const fx::Point& p) {
    dyn::ParameterSet params(s.reg);
    auto c = s.fs.contributions_at(fx::epoch_of(p), fx::state_of(p), params);
    if (!c) FAIL("contributions refused: " << c.error().id << ": " << c.error().message);
    return *c;
}

double inclination_rad(const fx::Point& p) { return (p.r_m > 1.0e7 ? 55.0 : 98.0) * 3.14159265358979323846 / 180.0; }

}  // namespace

TEST_CASE("FMOD-A-020: the truncation function -- the definition, the comparator's independence, the refusals", "[forcemodel][truncation]") {
    int points = 0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());

        Set a;
        fx::add_registered_set(a.fs, p, false, 0);
        const auto ca = contributions(a, p);
        auto smallest = forcemodel::smallest_nonzero_contribution(ca, {});
        REQUIRE(smallest.has_value());
        // the smallest non-zero norm, found here directly
        double manual = 1e300;
        std::string manual_name;
        for (const auto& c : ca) {
            const double n = c.acceleration_m_s2.norm();
            if (n > 0.0 && n < manual) { manual = n; manual_name = c.force.name; }
        }
        CHECK(smallest->value_m_s2 == manual);
        CHECK(smallest->name == manual_name);

        auto choice = forcemodel::degree_meeting_criterion(*field, p.r_m, smallest->value_m_s2, smallest->name);
        REQUIRE(choice.has_value());
        // the definition, from direct calls: the RMS at the degree is at most F_min, and one degree lower it is above (unless the degree is 2)
        auto rms_n = field->truncation_rms(p.r_m, fx::deg(choice->degree));
        REQUIRE(rms_n.has_value());
        CHECK(*rms_n <= smallest->value_m_s2);
        CHECK(*rms_n == choice->rms_at_degree);
        if (choice->degree > 2) {
            auto rms_below = field->truncation_rms(p.r_m, fx::deg(choice->degree - 1));
            REQUIRE(rms_below.has_value());
            CHECK(*rms_below > smallest->value_m_s2);
            CHECK(*rms_below == choice->rms_below);
        }

        // the comparator is independent of the truncated series: with Gravity registered at degree 2 and at degree 90, excluded by name, F_min is the same
        for (int gd : {2, 90}) {
            Set g;
            fx::add_registered_set(g.fs, p, false, gd);
            const auto cg = contributions(g, p);
            auto sg = forcemodel::smallest_nonzero_contribution(cg, {"gravity"});
            REQUIRE(sg.has_value());
            CHECK(sg->value_m_s2 == smallest->value_m_s2);
            CHECK(sg->name == smallest->name);
        }
        ++points;
    }
    REQUIRE(points == 4);

    // the refusals (FMOD-F-003)
    std::vector<dyn::Contribution> only_gravity{{dyn::ForceId{"gravity"}, Vec3{1.0, 0.0, 0.0}}};
    auto none = forcemodel::smallest_nonzero_contribution(only_gravity, {"gravity"});
    REQUIRE(!none.has_value());
    CHECK(none.error().id == "FMOD-F-003");
    std::vector<dyn::Contribution> zeros{{dyn::ForceId{"a"}, Vec3{}}, {dyn::ForceId{"b"}, Vec3{}}};
    CHECK(!forcemodel::smallest_nonzero_contribution(zeros, {}).has_value());
    auto field = fx::model().conventional(fx::epoch_of(fx::points()[1]), true);
    REQUIRE(field.has_value());
    auto bad = forcemodel::degree_meeting_criterion(*field, 7.0e6, 0.0);
    REQUIRE(!bad.has_value());
    CHECK(bad.error().id == "FMOD-F-003");
    CHECK(!forcemodel::degree_meeting_criterion(*field, -1.0, 1e-10).has_value());
}

TEST_CASE("FMOD-A-021: the truncation table at L4's four points, sets A and B, the orbit maximum beside the RMS, the predictions recorded",
          "[forcemodel][truncation]") {
    // The predictions, written BEFORE any measurement (SPEC-forcemodel §8): set A -- the manager's, ruling R4 -- GPS ~9 (8-11), 7331 km ~98 (90-110),
    // 720 km ~125 (105-150), 300 km ~275 (200-350); set B: N_B - N_A in [20, 100] at the three low points, in [0, 4] at GPS; the orbit maximum exceeds
    // the sphere's RMS by a factor in [1.5, 8].  RECORDED, NOT A CRITERION: a miss is reported as written.
    struct Prediction { double lo, nominal, hi; };
    const std::map<std::string, Prediction> set_a{{"GPS 26561 km", {8, 9, 11}}, {"LEO 952.86 km", {90, 98, 110}},
                                                  {"sail 720 km", {105, 125, 150}}, {"LEO 300 km", {200, 275, 350}}};
    std::ostringstream table;
    table << "FMOD-A-021 -- the truncation table (degree N: lowest N >= 2 with truncation_rms(r, N) <= F_min; the orbit maximum is the maximum over a 96-point "
             "circular orbit of |a_2190 - a_N|)\n";
    table << std::scientific << std::setprecision(3);
    int rows = 0;
    std::map<std::string, int> degree_a;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());
        std::vector<int> all;
        for (int n = 2; n <= field->max_degree(); ++n) all.push_back(n);
        auto maxima = forcemodel::orbit_tail_maxima(*field, p.r_m, inclination_rad(p), all, 96);
        REQUIRE(maxima.has_value());
        REQUIRE(maxima->size() == all.size());

        for (bool planets : {false, true}) {
            Set s;
            fx::add_registered_set(s.fs, p, planets, 0);
            const auto c = contributions(s, p);
            REQUIRE(c.size() == (planets ? 11u : 6u));      // A: Sun, Moon, tides, three relativity terms; B: and five planets
            auto smallest = forcemodel::smallest_nonzero_contribution(c, {});
            REQUIRE(smallest.has_value());
            auto choice = forcemodel::degree_meeting_criterion(*field, p.r_m, smallest->value_m_s2, smallest->name);
            REQUIRE(choice.has_value());
            const double orbit_max = (*maxima)[static_cast<std::size_t>(choice->degree - 2)];
            int n_orbit = field->max_degree();
            for (std::size_t i = 0; i < maxima->size(); ++i)
                if ((*maxima)[i] <= smallest->value_m_s2) { n_orbit = all[i]; break; }
            const double ratio = orbit_max / choice->rms_at_degree;
            // asserted: the definition (the logic of the function), and that B, which contains A, cannot need a lower degree
            CHECK(choice->rms_at_degree <= smallest->value_m_s2);
            if (!planets) degree_a[p.name] = choice->degree;
            else CHECK(choice->degree >= degree_a[p.name]);

            table << "  " << std::left << std::setw(14) << p.name << " set " << (planets ? "B" : "A") << ": N = " << std::setw(4) << choice->degree
                  << " rms(N) = " << choice->rms_at_degree << " rms(N-1) = " << choice->rms_below << "  F_min = " << smallest->value_m_s2
                  << " set by " << smallest->name << " (" << c.size() << " forces compared)  orbit max at N = " << orbit_max << " (x" << std::fixed
                  << std::setprecision(2) << ratio << std::scientific << std::setprecision(3) << " the RMS), orbit max meets F_min at N = " << n_orbit;
            if (!planets) {
                const auto& pr = set_a.at(p.name);
                table << "   prediction " << pr.nominal << " (" << pr.lo << "-" << pr.hi << "): "
                      << (choice->degree >= pr.lo && choice->degree <= pr.hi ? "MET" : "MISSED");
            } else {
                const int d = choice->degree - degree_a[p.name];
                const bool gps = p.r_m > 1.0e7;
                table << "   N_B - N_A = " << d << " (prediction " << (gps ? "[0, 4]" : "[20, 100]") << "): "
                      << ((gps ? (d >= 0 && d <= 4) : (d >= 20 && d <= 100)) ? "MET" : "MISSED");
            }
            table << "   orbit max / RMS " << std::fixed << std::setprecision(2) << ratio << " (prediction [1.5, 8]): " << (ratio >= 1.5 && ratio <= 8.0 ? "MET" : "MISSED")
                  << std::scientific << std::setprecision(3) << "\n";
            ++rows;
        }
    }
    REQUIRE(rows == 8);
    WARN(table.str());
}
