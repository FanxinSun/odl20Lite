#include <odl/tides/increments.hpp>
#include <odl/tides/wobble.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>

namespace odl::tides {

namespace {
constexpr double kArcsec = std::numbers::pi / (180.0 * 3600.0);
}

double Wobble::m1_arcsec() const noexcept { return m1_ / kArcsec; }
double Wobble::m2_arcsec() const noexcept { return m2_ / kArcsec; }

TideIncrements::TideIncrements(int max_degree, TideSystem system, ModelRecord record)
    : dc_(index(max_degree, max_degree) + 1, 0.0),
      ds_(index(max_degree, max_degree) + 1, 0.0),
      max_degree_(max_degree), system_(system), models_{std::move(record)} {}

void TideIncrements::add(int n, int m, double c, double s) noexcept {
    if (n < 0 || m < 0 || m > n || n > max_degree_) return;
    const std::size_t i = index(n, m);
    dc_[i] += c;
    ds_[i] += s;
}

double TideIncrements::dc(int n, int m) const noexcept {
    if (n < 0 || m < 0 || m > n || n > max_degree_) return 0.0;
    return dc_[index(n, m)];
}

double TideIncrements::ds(int n, int m) const noexcept {
    if (n < 0 || m < 0 || m > n || n > max_degree_) return 0.0;
    return ds_[index(n, m)];
}

odl::Result<TideIncrements, TidesError>
TideIncrements::sum(const std::vector<const TideIncrements*>& parts) {
    if (parts.empty()) {
        return odl::err(TidesError{"PERT-F-001", "no increments to sum"});
    }
    const TideSystem system = parts.front()->system();
    for (const auto* p : parts) {
        if (p->system() != system) {
            std::ostringstream m;
            m << "tide increments in different tide systems cannot be summed.\n"
                 "  one is " << gravity::name_of(system) << ", from "
              << parts.front()->models().front().model << "\n"
                 "  another is " << gravity::name_of(p->system()) << ", from "
              << p->models().front().model << "\n"
                 "  TN36-6 §6.2.2 step 3 reconciles them: with a ZERO-TIDE background field the\n"
                 "  permanent part must be removed from the solid Earth tide, by (6.13), and a\n"
                 "  model that has not had it removed is in the tide-free convention. Adding\n"
                 "  the two counts the permanent deformation twice.";
            return odl::err(TidesError{"PERT-F-001", m.str()});
        }
    }
    int nmax = 0;
    for (const auto* p : parts) nmax = std::max(nmax, p->max_degree());
    ModelRecord first = parts.front()->models().front();
    TideIncrements out{nmax, system, first};
    out.models_.clear();
    for (const auto* p : parts) {
        for (const auto& r : p->models()) out.models_.push_back(r);
        for (int n = 0; n <= p->max_degree(); ++n) {
            for (int m = 0; m <= n; ++m) out.add(n, m, p->dc(n, m), p->ds(n, m));
        }
    }
    return out;
}

}  // namespace odl::tides
