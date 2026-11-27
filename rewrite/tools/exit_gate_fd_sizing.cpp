// exit_gate_fd_sizing.cpp — the L7 exit gate's finite-difference sizing and shadow check, frozen BEFORE any exit-gate fit runs.
//
// SPEC-estimation.md v0.7: EST-R-904 (the shadow function over the arc), EST-R-905 (the sizing in the form of step 1: central differences, eps(h) = h^2 F/6 + nu/h, nu measured from model calls only, F from a scan of
// model calls) and EST-R-906's second-difference sizing; EST-A-901 / EST-A-902 read the figures this tool prints and writes.  Ruled by odl Committee 2026-11-27 (TREE-14): EST-Q-010 (a), EST-Q-011, EST-Q-012, EST-Q-013.
//
// WHAT IT DOES, per row G-01 (G01, 2023-01-22), G-02 (G01, 2023-01-23), G-03 (G05, 2023-01-22), for the CANNONBALL fit (the fit EST-A-901 and -902 gate):
//   1. reads the pinned SP3 file whole (96 epochs 900 s apart, the satellite present and unflagged at every epoch, else exit 1: EST-F-901);
//   2. builds the starting seven-parameter vector of SPEC-estimation 3.9 (the position of epoch 48 and the velocity of the degree-8 Lagrange polynomial through epochs 44-52, in the ITRS, carried to the GCRS; srp_scale = 1);
//   3. evaluates the residual vector (288 numbers: the ITRS position of the 96 epochs from the model minus the file's, metres) by chained propagation from the middle epoch, backwards for epochs 0-47 and forwards for 49-95,
//      with Gravity degree 10 (secular pole extrapolated), the Sun and the Moon, the solid / FES2004 (degree 7) / solid-pole tides, the three relativity terms, and Srp over a cannonball carried by ScaledForce (EST-R-901);
//   4. the shadow function over the arc: nu at the 96 file epochs and every 10 s on the nominal propagated trajectory (cubic Hermite between its epochs), minimum nu and the count below 1 (EST-Q-013);
//   5. the cannonball's nominal area and mass (EST-Q-011) and every surface of the box-wing macromodel it was taken from, printed;
//   6. the noise nu of the residual from model calls only, the scan of the third and fourth differences (a ladder of steps), the steps h_j, the bounds eps_j, the second-difference steps h2_j and bounds eps2_j,
//      the central-difference Jacobian at the start, delta_FD, and the indicative condition number of its scaled normal matrix.
//
// NO FIT IS RUN: the residual is evaluated at the start and at points around it.  Standard C++ only; every number printed is computed here and copied nowhere.
//
//   exit_gate_fd_sizing [--write-header FILE] [--arcs N] [--probe]      (--arcs N: only the first N rows; --probe: read the first row, evaluate its residual ONCE at the start, print the time and S_N, stop: no sizing)
//   exit 0 printed   1 the data are not whole (EST-F-901)   2 an argument or file error   70 an error the tool did not anticipate

#include <odl/core/units.hpp>
#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/forcemodel/earth_orientation.hpp>
#include <odl/forcemodel/gravity_force.hpp>
#include <odl/forcemodel/relativity_force.hpp>
#include <odl/forcemodel/scaled_force.hpp>
#include <odl/forcemodel/third_body_force.hpp>
#include <odl/forcemodel/tides_force.hpp>
#include <odl/frames/transform.hpp>
#include <odl/gravity/field.hpp>
#include <odl/gravity/scaling.hpp>
#include <odl/io/sp3.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/shadow/conical.hpp>
#include <odl/spacecraft/spacecraft.hpp>
#include <odl/srp/srp.hpp>
#include <odl/thirdbody/attraction.hpp>
#include <odl/tides/ocean.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace dyn = odl::dyn;
namespace fm = odl::forcemodel;
using odl::Vec3;

constexpr int kEpochs = 96;
constexpr int kMid = 48;                       // epoch index of 12:00:00 of the file's grid (EST-Q-006)
constexpr double kSpacingS = 900.0;
constexpr double kTolerance = 1e-12;           // the integrator tolerance of every model call (registered here, frozen with the sizing)
constexpr std::size_t kNp = 7;                 // [x, y, z, vx, vy, vz, srp_scale]
constexpr std::size_t kRows = static_cast<std::size_t>(kEpochs) * 3;
constexpr int kLadder = 4;                     // scan steps s0 * 4^m, m = 0..3
constexpr double kCannonballCr = 1.0;          // C_R of the cannonball (EST-Q-011): a perfect absorber, absorptivity 1

[[noreturn]] void die(int code, const std::string& msg) {
    std::fprintf(stderr, "exit_gate_fd_sizing: %s\n", msg.c_str());
    std::exit(code);
}
template <class R>
auto need(R&& r, const char* what) {
    if (!r.has_value()) die(2, std::string(what) + ": " + std::string(r.error().id) + " " + std::string(r.error().message));
    return std::move(*r);
}

template <class R>
void check(R&& r, const char* what) {
    if (!r.has_value()) die(2, std::string(what) + ": " + std::string(r.error().id) + " " + std::string(r.error().message));
}

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) die(2, "cannot read " + path);
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The pinned inputs, as the force-model tests read them (the paths and hashes are compile definitions from the manifest).
struct World {
    odl::time::LeapTable leaps = need(odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""}), "leap seconds");
    odl::eph::Ephemeris ephemeris = need(odl::eph::Ephemeris::open({ODL_DE440S_BSP}, {}), "ephemeris");
    odl::thirdbody::GravitationalParameters gm = need(odl::thirdbody::GravitationalParameters::load(ODL_GM_DE440_TPC, ODL_MANIFEST_CACHE_ROOT), "GM");
    odl::gravity::GravityModel model = need(odl::gravity::GravityModel::load(ODL_EGM2008_COEFFICIENTS, ODL_MANIFEST_CACHE_ROOT, odl::gravity::ScalingParameters::egm2008_tt_compatible()), "EGM2008");
    odl::eop::EopSeries c04 = need(odl::eop::EopSeries::load_c04(slurp(ODL_C04_FILE), odl::eop::EopProvenance{"eop-c04-20", "", "", ""}, leaps), "EOP C04");
    odl::tides::OceanTide fes = need(odl::tides::OceanTide::load(ODL_FES2004_CNM_SNM, ODL_MANIFEST_CACHE_ROOT), "FES2004");
    std::vector<fm::SourceRecord> sources() const {
        return {{"egm2008-coefficients", ODL_EGM2008_SHA256}, {"naif-gm-de440", ODL_GM_DE440_SHA256}, {"de440s-spk", ODL_DE440S_SHA256},
                {"eop-c04-20", ODL_C04_SHA256},              {"fes2004-ocean-tide", ODL_FES2004_SHA256}};
    }
};

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The data of one row.
struct Arc {
    const char* name;
    const char* sat;
    const char* file;
    bool iif;            // G01 = SVN63 = Block IIF; G05 = SVN50 = Block IIR-M (PROVENANCE.md 30.2)
};

struct Data {
    std::vector<odl::time::Epoch> t;
    std::vector<Vec3> p_itrs_m;
};

Data read_data(const World& w, const Arc& arc) {
    auto sp3 = need(odl::io::read_sp3(slurp(arc.file)), "read_sp3");
    if (sp3.epochs.size() != static_cast<std::size_t>(kEpochs)) die(1, std::string("EST-F-901: ") + arc.name + ": " + std::to_string(sp3.epochs.size()) + " epochs, not 96");
    if (sp3.header.pos_vel_flag != odl::io::Sp3PosVelFlag::Position) die(1, std::string("EST-F-901: ") + arc.name + ": the header flag is not P");
    const auto scale = need(odl::io::to_time_scale(sp3.header.time_system), "time system");
    Data d;
    for (std::size_t k = 0; k < sp3.epochs.size(); ++k) {
        const auto& ep = sp3.epochs[k];
        auto t = need(odl::time::Epoch::from_calendar(scale, ep.epoch, w.leaps), "epoch");
        if (k > 0 && std::fabs(t.difference(d.t.back()).to_seconds() - kSpacingS) > 1e-6) die(1, std::string("EST-F-901: ") + arc.name + ": epoch " + std::to_string(k) + " is not 900 s after the one before");
        const odl::io::Sp3PositionRecord* rec = nullptr;
        for (const auto& s : ep.satellites)
            if (s.position.satellite_id == arc.sat) rec = &s.position;
        if (!rec) die(1, std::string("EST-F-901: ") + arc.name + ": " + arc.sat + " absent at epoch " + std::to_string(k));
        if (rec->x_km >= 999999.0 || rec->y_km >= 999999.0 || rec->z_km >= 999999.0) die(1, std::string("EST-F-901: ") + arc.name + ": " + arc.sat + " flagged bad at epoch " + std::to_string(k));
        d.t.push_back(t);
        d.p_itrs_m.push_back(odl::metres_from_km(Vec3{rec->x_km, rec->y_km, rec->z_km}));
    }
    return d;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The cannonball macromodel of a row: the box-wing block's sun-pointing surfaces' total area (the panels, normal to the Sun by the tracking law: zero off-sun angle), the block's cited mass, C_R = 1.
struct Cannonball {
    double panel_area_m2 = 0, bus_area_m2 = 0, mass_kg = 0;
    std::string listing;
};

Cannonball describe(const Arc& arc) {
    auto block = need(arc.iif ? odl::spacecraft::gps_block_iif() : odl::spacecraft::gps_block_iir_m(), "box-wing macromodel");
    Cannonball c;
    c.mass_kg = block.mass_kg().value();
    char line[256];
    int n = 0;
    for (const auto& s : block.surfaces()) {
        ++n;
        if (const auto* f = std::get_if<odl::macromodel::FlatSurface>(&s)) {
            const bool sunp = f->normal_mode() == odl::macromodel::NormalMode::sun_pointing;
            (sunp ? c.panel_area_m2 : c.bus_area_m2) += f->area_m2().value();
            std::snprintf(line, sizeof line, "    surface %d: flat, %s, area %.17g m^2\n", n, sunp ? "sun-pointing" : "body-fixed", f->area_m2().value());
        } else {
            std::snprintf(line, sizeof line, "    surface %d: spherical\n", n);
        }
        c.listing += line;
    }
    return c;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Model {
    const World& w;
    const Data& d;
    dyn::ParameterRegistry registry;
    dyn::ParameterId c_id;
    std::unique_ptr<dyn::ForceSet> set;

    Model(const World& world, const Data& data, double area_m2, double mass_kg, int degree)
        : w(world), d(data), c_id(registry.declare({dyn::ParameterKind::srp_scale, "1", "srp_scale", "cannonball"})), set(std::make_unique<dyn::ForceSet>(registry)) {
        odl::forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        go.sources = w.sources();
        auto dg = need(odl::gravity::Degree::of(degree), "degree");
        auto og = need(odl::gravity::Order::of(degree), "order");
        const auto eo = odl::forcemodel::EarthOrientation::from_series(w.c04, odl::eop::EopPolicy{}, w.leaps);
        add(std::make_shared<odl::forcemodel::Gravity>(w.model, eo, dg, og, go));
        const std::vector<odl::eph::Body> bodies{odl::eph::Body::Sun, odl::eph::Body::Moon};
        auto tb = need(odl::forcemodel::third_bodies(bodies, w.ephemeris, w.gm, w.leaps, w.sources()), "third bodies");
        for (auto& f : tb) add(f);
        odl::forcemodel::TidesOptions to;
        to.ocean = &w.fes;
        to.ocean_degree = 7;                       // TN36-6 Table 6.1's value at GPS
        to.extrapolate_secular = true;
        to.sources = w.sources();
        add(std::make_shared<odl::forcemodel::Tides>(w.model, eo, w.ephemeris, w.leaps, to));
        for (auto term : {odl::relativity::Term::Schwarzschild, odl::relativity::Term::LenseThirring, odl::relativity::Term::DeSitter})
            add(std::make_shared<odl::forcemodel::Relativity>(term, w.ephemeris, w.leaps, odl::relativity::PpnParameters::general_relativity(), w.sources()));
        auto area = need(odl::macromodel::cited(area_m2, "the box-wing macromodel's sun-pointing surfaces' total area (EST-Q-011, odl Committee 2026-11-27)"), "area");
        auto ab = need(odl::macromodel::cited(kCannonballCr, "C_R = 1: a perfect absorber (EST-Q-011)"), "absorptivity");
        auto sp = need(odl::macromodel::cited(0.0, "perfect absorber"), "specular");
        auto df = need(odl::macromodel::cited(0.0, "perfect absorber"), "diffuse");
        odl::macromodel::MacromodelBuilder b;
        b.add_surface(odl::macromodel::SphericalSurface{area, ab, sp, df, std::nullopt});
        auto m = need(odl::macromodel::cited(mass_kg, "the block's cited mass (SPEC-spacecraft; PROVENANCE.md 30.2)"), "mass");
        auto com = need(odl::macromodel::cited(Vec3{0.0, 0.0, 0.0}, "cannonball: the centre of mass is the centre"), "centre of mass");
        b.set_mass(m).set_centre_of_mass(com);
        auto mm = need(std::move(b).build(), "macromodel");
        std::shared_ptr<const dyn::Force> srp = std::make_shared<odl::srp::Srp>(mm, w.ephemeris, w.leaps);
        auto scaled = need(odl::forcemodel::ScaledForce::create(srp, c_id, registry), "ScaledForce");
        add(scaled);
    }
    void add(std::shared_ptr<dyn::Force> f) { check(set->add(std::move(f)), "ForceSet::add"); }

    static odl::frames::GcrsState state_of(const odl::time::Epoch& t, const double* x) {
        return odl::frames::GcrsState{t, odl::km_from_metres(Vec3{x[0], x[1], x[2]}), odl::km_from_metres(Vec3{x[3], x[4], x[5]})};
    }

    /// The residual (ITRS metres, model minus file) of theta, and optionally the GCRS states at the 96 epochs.
    std::vector<double> residual(const std::array<double, kNp>& theta, std::vector<odl::frames::GcrsState>* states = nullptr) const {
        dyn::ParameterSet params{registry};
        check(params.set(c_id, theta[6]), "parameter set");
        std::vector<odl::frames::GcrsState> st(static_cast<std::size_t>(kEpochs), state_of(d.t[static_cast<std::size_t>(kMid)], theta.data()));
        for (int k = kMid - 1; k >= 0; --k) {
            const auto ku = static_cast<std::size_t>(k);
            st[ku] = need(dyn::propagate(*set, params, st[ku + 1], d.t[ku].difference(d.t[ku + 1]).to_seconds(), kTolerance), "propagate (backward)");
        }
        for (int k = kMid + 1; k < kEpochs; ++k) {
            const auto ku = static_cast<std::size_t>(k);
            st[ku] = need(dyn::propagate(*set, params, st[ku - 1], d.t[ku].difference(d.t[ku - 1]).to_seconds(), kTolerance), "propagate (forward)");
        }
        std::vector<double> r;
        r.reserve(kRows);
        for (std::size_t k = 0; k < st.size(); ++k) {
            const auto rec = need(w.c04.at(d.t[k], odl::eop::EopPolicy{}), "EOP at epoch");
            const auto it = need(odl::frames::to_itrs(st[k], rec, w.leaps), "to_itrs");
            const Vec3 q = odl::metres_from_km(it.position());
            r.push_back(q.x - d.p_itrs_m[k].x);
            r.push_back(q.y - d.p_itrs_m[k].y);
            r.push_back(q.z - d.p_itrs_m[k].z);
        }
        if (states) *states = std::move(st);
        return r;
    }
};

// The nominal start (SPEC-estimation 3.9): position of epoch 48; velocity of the degree-8 Lagrange polynomial through epochs 44-52, differentiated at 48 (the nine-point central formula), ITRS -> GCRS.
std::array<double, kNp> nominal_start(const World& w, const Data& d) {
    static const double wgt[9] = {1.0 / 280.0, -4.0 / 105.0, 1.0 / 5.0, -4.0 / 5.0, 0.0, 4.0 / 5.0, -1.0 / 5.0, 4.0 / 105.0, -1.0 / 280.0};
    Vec3 v{0, 0, 0};
    for (int i = 0; i < 9; ++i) {
        const Vec3& p = d.p_itrs_m[static_cast<std::size_t>(kMid - 4 + i)];
        v.x += wgt[i] * p.x;
        v.y += wgt[i] * p.y;
        v.z += wgt[i] * p.z;
    }
    v = (1.0 / kSpacingS) * v;
    const auto t = d.t[static_cast<std::size_t>(kMid)];
    odl::frames::ItrsState s{t, odl::km_from_metres(d.p_itrs_m[static_cast<std::size_t>(kMid)]), odl::km_from_metres(v)};
    const auto rec = need(w.c04.at(t, odl::eop::EopPolicy{}), "EOP at the middle epoch");
    const auto g = need(odl::frames::to_gcrs(s, rec, w.leaps), "to_gcrs");
    const Vec3 r = odl::metres_from_km(g.position()), vv = odl::metres_from_km(g.velocity());
    return {r.x, r.y, r.z, vv.x, vv.y, vv.z, 1.0};
}

double max_abs_diff(const std::vector<double>& a, const std::vector<double>& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}
double norm2(const std::vector<double>& a) {
    double s = 0.0;
    for (double x : a) s += x * x;
    return std::sqrt(s);
}

// ---- the shadow function over the arc ------------------------------------------------------------------------------------------------------------------------------------------------------------
Vec3 sun_m(const World& w, const odl::time::Epoch& t) {
    auto s = need(w.ephemeris.geocentric_state(odl::eph::Body::Sun, t, w.leaps), "Sun");
    return odl::metres_from_km(s.position());
}
double nu_at(const World& w, const odl::time::Epoch& t, const Vec3& r_m) {
    auto n = need(odl::shadow::conical(odl::frames::Position<odl::frames::Frame::GCRS>{sun_m(w, t)}, odl::frames::Position<odl::frames::Frame::GCRS>{r_m}), "shadow function");
    return n.fraction;
}
Vec3 hermite(const std::array<double, 6>& a, const std::array<double, 6>& b, double h, double u) {
    const double u2 = u * u, u3 = u2 * u;
    const double h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
    return Vec3{h00 * a[0] + h10 * h * a[3] + h01 * b[0] + h11 * h * b[3], h00 * a[1] + h10 * h * a[4] + h01 * b[1] + h11 * h * b[4], h00 * a[2] + h10 * h * a[5] + h01 * b[2] + h11 * h * b[5]};
}
std::array<double, 6> metres_of(const odl::frames::GcrsState& s) {
    const Vec3 r = odl::metres_from_km(s.position()), v = odl::metres_from_km(s.velocity());
    return {r.x, r.y, r.z, v.x, v.y, v.z};
}

// ---- a small symmetric eigen-solver (cyclic Jacobi) for the indicative condition number -----------------------------------------------------------------------------------------------------------
std::array<double, kNp> eigenvalues(std::array<std::array<double, kNp>, kNp> a) {
    for (int sweep = 0; sweep < 100; ++sweep) {
        double off = 0.0;
        for (std::size_t i = 0; i < kNp; ++i)
            for (std::size_t j = i + 1; j < kNp; ++j) off += a[i][j] * a[i][j];
        if (off < 1e-30) break;
        for (std::size_t p = 0; p < kNp; ++p)
            for (std::size_t q = p + 1; q < kNp; ++q) {
                if (std::fabs(a[p][q]) < 1e-300) continue;
                const double th = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (th >= 0 ? 1.0 : -1.0) / (std::fabs(th) + std::sqrt(th * th + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0), s = t * c;
                for (std::size_t k = 0; k < kNp; ++k) {
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for (std::size_t k = 0; k < kNp; ++k) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
            }
    }
    std::array<double, kNp> e{};
    for (std::size_t i = 0; i < kNp; ++i) e[i] = a[i][i];
    return e;
}

struct Sizing {
    double nu = 0;
    std::array<double, kNp> f3{}, h{}, eps{}, f4{}, h2{}, eps2{};
    double delta_fd = 0, kappa_start = 0, tol_start = 0;
};

const char* const kNames[kNp] = {"x", "y", "z", "vx", "vy", "vz", "srp_scale"};
const double kS0[kNp] = {1.0, 1.0, 1.0, 1e-3, 1e-3, 1e-3, 1e-3};      // the first step of each parameter's ladder, in its own units (m, m, m, m/s, m/s, m/s, 1)

Sizing size_arc(const Model& model, const std::array<double, kNp>& th0, const std::vector<double>& r0) {
    Sizing z;
    auto moved = [&](std::size_t j, double dx) {
        auto t = th0;
        t[j] += dx;
        return model.residual(t);
    };
    // nu: one repeat of the start, and each parameter moved by an amount far below any step (1e-6 s0)
    double nu = max_abs_diff(model.residual(th0), r0);
    std::printf("  noise: repeat of the start: max |dr| = %.17g m\n", nu);
    for (std::size_t j = 0; j < kNp; ++j) {
        const double d = max_abs_diff(moved(j, 1e-6 * kS0[j]), r0);
        std::printf("  noise: %s moved by %.17g: max |dr| = %.17g m\n", kNames[j], 1e-6 * kS0[j], d);
        nu = std::max(nu, d);
    }
    z.nu = nu;
    std::printf("  nu = %.17g m (the largest spread)\n", nu);
    const double sig3 = 100.0 * nu, sig4 = 200.0 * nu;            // a difference counts as signal when it exceeds this multiple of nu
    for (std::size_t j = 0; j < kNp; ++j) {
        double f3 = 0.0, f4 = 0.0;
        bool got3 = false, got4 = false;
        double stop = 0.0;
        for (int m = 0; m < kLadder; ++m) {
            const double s = kS0[j] * std::pow(4.0, m);
            stop = s;
            const auto p1 = moved(j, s), m1 = moved(j, -s), p2 = moved(j, 2 * s), m2 = moved(j, -2 * s);
            double n3 = 0.0, n4 = 0.0;
            for (std::size_t i = 0; i < kRows; ++i) {
                const double d3 = p2[i] - 2 * p1[i] + 2 * m1[i] - m2[i];
                const double d4 = p2[i] - 4 * p1[i] + 6 * r0[i] - 4 * m1[i] + m2[i];
                n3 = std::max(n3, std::fabs(d3));
                n4 = std::max(n4, std::fabs(d4));
            }
            const double q3 = n3 / (2 * s * s * s), q4 = n4 / (s * s * s * s);
            const bool u3 = n3 >= sig3, u4 = n4 >= sig4;
            std::printf("  scan %s s = %.17g: max|third diff| = %.17g -> D3 = %.17g %s; max|fourth diff| = %.17g -> D4 = %.17g %s\n", kNames[j], s, n3, q3, u3 ? "(signal)" : "(below 100 nu)", n4, q4, u4 ? "(signal)" : "(below 200 nu)");
            if (u3) { f3 = std::max(f3, q3); got3 = true; }
            if (u4) { f4 = std::max(f4, q4); got4 = true; }
        }
        if (!got3) f3 = sig3 / (2 * stop * stop * stop);              // nothing above the noise: the largest third derivative that could hide there
        if (!got4) f4 = sig4 / (stop * stop * stop * stop);
        z.f3[j] = f3;
        z.f4[j] = f4;
        z.h[j] = std::min(std::cbrt(3.0 * nu / f3), stop);
        z.eps[j] = z.h[j] * z.h[j] * f3 / 6.0 + nu / z.h[j];
        z.h2[j] = std::min(std::pow(48.0 * nu / f4, 0.25), stop);
        z.eps2[j] = z.h2[j] * z.h2[j] * f4 / 12.0 + 4.0 * nu / (z.h2[j] * z.h2[j]);
        std::printf("  %s: F3 = %.17g%s, h = %.17g, eps = %.17g | F4 = %.17g%s, h2 = %.17g, eps2 = %.17g\n", kNames[j], f3, got3 ? "" : " (bound)", z.h[j], z.eps[j], f4, got4 ? "" : " (bound)", z.h2[j], z.eps2[j]);
    }
    // the central-difference Jacobian at the start, delta_FD, the indicative condition number of its scaled normal matrix
    std::vector<std::vector<double>> col(kNp);
    std::array<double, kNp> cn{};
    for (std::size_t j = 0; j < kNp; ++j) {
        const auto up = moved(j, z.h[j]), dn = moved(j, -z.h[j]);
        col[j].resize(kRows);
        for (std::size_t i = 0; i < kRows; ++i) col[j][i] = (up[i] - dn[i]) / (2.0 * z.h[j]);
        cn[j] = norm2(col[j]);
        std::printf("  J_FD column %s: norm %.17g, eps_j/norm = %.17g\n", kNames[j], cn[j], z.eps[j] / cn[j]);
        z.delta_fd = std::max(z.delta_fd, z.eps[j] / cn[j]);
    }
    std::array<std::array<double, kNp>, kNp> a{};
    for (std::size_t i = 0; i < kNp; ++i)
        for (std::size_t j = 0; j < kNp; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < kRows; ++k) s += col[i][k] * col[j][k];
            a[i][j] = s / (cn[i] * cn[j]);
        }
    const auto ev = eigenvalues(a);
    const double lo = *std::min_element(ev.begin(), ev.end()), hi = *std::max_element(ev.begin(), ev.end());
    z.kappa_start = hi / lo;
    z.tol_start = std::max(1e-6, 8.0 * z.kappa_start * z.delta_fd);
    std::printf("  delta_FD = max_j eps_j/|J_FD,j| = %.17g\n  scaled N_FD at the start: lambda_min = %.17g, lambda_max = %.17g, kappa_2 = %.17g (INDICATIVE; the gate uses the fit's own ConditionReport::condition_estimate)\n", z.delta_fd, lo, hi, z.kappa_start);
    std::printf("  tol_ij at the start = max(1e-6, 8 kappa delta_FD) = %.17g (INDICATIVE)\n", z.tol_start);
    return z;
}

void write_arr(std::FILE* f, const char* name, const std::array<double, kNp>& a) {
    (void)name; std::fprintf(f, "    {");
    for (std::size_t i = 0; i < kNp; ++i) std::fprintf(f, "%s%.17g", i ? ", " : "", a[i]);
    std::fprintf(f, "},\n");
}

}  // namespace

int main(int argc, char** argv) {
    const char* header_path = nullptr;
    int arcs = 3;
    bool probe = false;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--write-header") && i + 1 < argc) header_path = argv[++i];
        else if (!std::strcmp(argv[i], "--arcs") && i + 1 < argc) arcs = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--probe")) probe = true;
        else die(2, std::string("unknown argument ") + argv[i]);
    }
    const std::string c22 = std::string(ODL_CACHE_DIR_STR) + "/igs-final-sp3-2023-022/extracted/IGS0OPSFIN_20230220000_01D_15M_ORB.SP3";
    const std::string c23 = std::string(ODL_CACHE_DIR_STR) + "/igs-final-sp3-2023-023/extracted/IGS0OPSFIN_20230230000_01D_15M_ORB.SP3";
    const Arc rows[3] = {{"G-01", "G01", c22.c_str(), true}, {"G-02", "G01", c23.c_str(), true}, {"G-03", "G05", c22.c_str(), false}};
    std::printf("exit_gate_fd_sizing: SPEC-estimation v0.7, EST-R-904 / -905 / -906; cannonball fits; integrator tolerance %.17g; ladder s0*4^m, m = 0..%d; NO FIT IS RUN\n", kTolerance, kLadder - 1);
    const World w;
    std::FILE* hf = header_path ? std::fopen(header_path, "w") : nullptr;
    if (header_path && !hf) die(2, std::string("cannot write ") + header_path);
    if (hf) std::fprintf(hf, "#pragma once\n// GENERATED by tools/exit_gate_fd_sizing (SPEC-estimation v0.7 EST-R-905): the frozen finite-difference sizing of the exit gate's cannonball fits.  Do not edit; the fit may not change a figure.\n"
                             "// Units: nu, eps, eps2, h, h2 in metres (state components), metres per second (velocity components) or 1 (srp_scale).\n#include <array>\nnamespace odl::l7exit {\nstruct FrozenSizing {\n    const char* row; double nu; std::array<double, 7> f3, h, eps, f4, h2, eps2; double delta_fd;\n};\ninline constexpr FrozenSizing kFrozen[] = {\n");
    int shadow_cross = 0;
    for (int ai = 0; ai < arcs; ++ai) {
        const Arc& arc = rows[ai];
        std::printf("\n== %s: %s on %s ==\n", arc.name, arc.sat, arc.file);
        const Data d = read_data(w, arc);
        std::printf("  data: %zu epochs, %s present and unflagged at every epoch, spacing 900 s\n", d.t.size(), arc.sat);
        const Cannonball cb = describe(arc);
        std::printf("  box-wing macromodel (%s):\n%s", arc.iif ? "gps_block_iif(), SVN63" : "gps_block_iir_m(), SVN50", cb.listing.c_str());
        std::printf("  cannonball: area = %.17g m^2 (the sun-pointing surfaces' total; the body-fixed faces' total %.17g m^2 is NOT added), mass = %.17g kg, C_R = %.17g, A C_R / m = %.17g m^2/kg\n", cb.panel_area_m2, cb.bus_area_m2, cb.mass_kg, kCannonballCr, cb.panel_area_m2 * kCannonballCr / cb.mass_kg);
        const Model model(w, d, cb.panel_area_m2, cb.mass_kg, 10);
        const auto th0 = nominal_start(w, d);
        std::printf("  start: x = %.17g %.17g %.17g m, v = %.17g %.17g %.17g m/s, srp_scale = %.17g\n", th0[0], th0[1], th0[2], th0[3], th0[4], th0[5], th0[6]);
        std::vector<odl::frames::GcrsState> st;
        const auto t_begin = std::chrono::steady_clock::now();
        const auto r0 = model.residual(th0, &st);
        if (probe) {
            std::printf("  probe: one residual evaluation took %.3f s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - t_begin).count());
            double q2 = 0.0;
            for (double x : r0) q2 += x * x;
            std::printf("  probe: S_N at the start = %.17g m\n", std::sqrt(q2 / kEpochs));
            return 0;
        }
        double s2 = 0.0;
        for (double x : r0) s2 += x * x;
        std::printf("  S_N at the start = %.17g m (N = %d)\n", std::sqrt(s2 / kEpochs), kEpochs);
        // the shadow function: the 96 file epochs and every 10 s on the nominal trajectory
        double min_nu_file = 1.0, min_nu_fine = 1.0;
        int below_file = 0, below_fine = 0, samples = 0;
        for (std::size_t k = 0; k < st.size(); ++k) {
            const double nu = nu_at(w, d.t[k], odl::metres_from_km(st[k].position()));
            min_nu_file = std::min(min_nu_file, nu);
            if (nu < 1.0) ++below_file;
        }
        for (std::size_t k = 0; k + 1 < st.size(); ++k) {
            const auto a = metres_of(st[k]), b = metres_of(st[k + 1]);
            for (int j = 0; j < 90; ++j) {
                const double u = j / 90.0;
                const auto t = d.t[k].add(odl::time::Duration::from_seconds(u * kSpacingS));
                const double nu = nu_at(w, t, hermite(a, b, kSpacingS, u));
                min_nu_fine = std::min(min_nu_fine, nu);
                if (nu < 1.0) ++below_fine;
                ++samples;
            }
        }
        const bool crosses = below_file > 0 || below_fine > 0;
        if (crosses) ++shadow_cross;
        std::printf("  shadow: file epochs: min nu = %.17g, %d of %d below 1; every 10 s (%d samples, Hermite between the nominal trajectory's epochs): min nu = %.17g, %d below 1 -> %s\n", min_nu_file, below_file, kEpochs, samples, min_nu_fine, below_fine, crosses ? "CROSSES THE SHADOW (EST-A-901 printed, not gated: EST-Q-013)" : "nu = 1 THROUGHOUT (EST-A-901 gated)");
        const Sizing z = size_arc(model, th0, r0);
        if (hf) {
            std::fprintf(hf, "  {\"%s\", %.17g,\n", arc.name, z.nu);
            write_arr(hf, "f3", z.f3); write_arr(hf, "h", z.h); write_arr(hf, "eps", z.eps); write_arr(hf, "f4", z.f4); write_arr(hf, "h2", z.h2); write_arr(hf, "eps2", z.eps2);
            std::fprintf(hf, "    %.17g},\n", z.delta_fd);
        }
    }
    if (hf) {
        std::fprintf(hf, "};\n}  // namespace odl::l7exit\n");
        std::fclose(hf);
    }
    std::printf("\nshadow summary: %d of %d rows cross the shadow\nexit 0\n", shadow_cross, arcs);
    return 0;
}
