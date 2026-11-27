#pragma once
// exit_gate_model.hpp — the L7 exit gate's model (SPEC-estimation.md v0.7, §3.9; EST-R-901 – EST-R-904): the pinned inputs, the data of a row, and the seven-parameter residual and its Jacobian.
//
// THE MODEL.  theta = [x, y, z, vx, vy, vz, srp_scale]: the GCRS state at the file's middle epoch (epoch 48, EST-Q-006) in metres and metres per second, and the SRP scale (EST-R-901: ScaledForce over Srp).
// The state of epoch k is the chained propagation from epoch k+1 (backwards, k < 48) or k-1 (forwards, k > 48) at integrator tolerance 1e-12; the observation of epoch k is the ITRS position of the SP3 file,
// metres, unit weight; the LM residual is r = p - q(theta) and the Jacobian is dq/dtheta (the partials of the model, as step 3 states).  The analytic Jacobian is the chain of propagate_with_sensitivities:
// Phi(t_k, t_mid) = Phi_step Phi_prev and S_k = Phi_step S_prev + s_step; the ITRS position is M r (M = the GCRS -> ITRS rotation), so dq/dtheta_state = M Phi_r and dq/dc = (metres per km) M S_r.
// The same code serves tools/exit_gate_fd_sizing.cpp's model by copy: the tool is standalone, so that its figures depend on no test code.
#include <catch2/catch_test_macros.hpp>

#include <odl/core/units.hpp>
#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/eop/series.hpp>
#include <odl/ephemerides/ephemeris.hpp>
#include <odl/estimation/levenberg_marquardt.hpp>
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
#include <odl/tides/pole.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace l7exit {

namespace dyn = odl::dyn;
namespace est = odl::estimation;
using odl::Vec3;

inline constexpr int kEpochs = 96, kMid = 48;
inline constexpr double kSpacingS = 900.0, kTolerance = 1e-12;
inline constexpr std::size_t kNp = 7, kRows = 288;
inline constexpr double kCr = 1.0;
using Theta = std::array<double, kNp>;

struct Row {
    const char* name;
    const char* sat;
    const char* file;
    bool iif;       // G01 = SVN63 = Block IIF; G05 = SVN50 = Block IIR-M (PROVENANCE.md 30.2)
    double F;       // the frozen number, metres
};
inline const std::array<Row, 3>& rows() {
    static const std::array<Row, 3> r{{{"G-01", "G01", ODL_SP3_022, true, 0.0645}, {"G-02", "G01", ODL_SP3_023, true, 0.0573}, {"G-03", "G05", ODL_SP3_022, false, 0.1870}}};
    return r;
}

inline std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}
template <class R>
auto must(R&& r, const char* what) {
    if (!r.has_value()) FAIL(what << ": " << std::string(r.error().id) << " " << std::string(r.error().message));
    return std::move(*r);
}

struct World {
    odl::time::LeapTable leaps = must(odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""}), "leap seconds");
    odl::eph::Ephemeris ephemeris = must(odl::eph::Ephemeris::open({ODL_DE440S_BSP}, {}), "ephemeris");
    odl::thirdbody::GravitationalParameters gm = must(odl::thirdbody::GravitationalParameters::load(ODL_GM_DE440_TPC, ODL_MANIFEST_CACHE_ROOT), "GM");
    odl::gravity::GravityModel model = must(odl::gravity::GravityModel::load(ODL_EGM2008_COEFFICIENTS, ODL_MANIFEST_CACHE_ROOT, odl::gravity::ScalingParameters::egm2008_tt_compatible()), "EGM2008");
    odl::eop::EopSeries c04 = must(odl::eop::EopSeries::load_c04(slurp(ODL_C04_FILE), odl::eop::EopProvenance{"eop-c04-20", "", "", ""}, leaps), "EOP C04");
    odl::tides::OceanTide fes = must(odl::tides::OceanTide::load(ODL_FES2004_CNM_SNM, ODL_MANIFEST_CACHE_ROOT), "FES2004");
    odl::tides::OceanPoleTide desai = must(odl::tides::OceanPoleTide::load(ODL_DESAI_POLE_COEF, ODL_MANIFEST_CACHE_ROOT), "Desai ocean pole tide");
    std::vector<odl::forcemodel::SourceRecord> sources() const {
        return {{"egm2008-coefficients", ODL_EGM2008_SHA256}, {"naif-gm-de440", ODL_GM_DE440_SHA256}, {"de440s-spk", ODL_DE440S_SHA256},
                {"eop-c04-20", ODL_C04_SHA256},              {"fes2004-ocean-tide", ODL_FES2004_SHA256}, {"desai-ocean-pole-tide", ODL_DESAI_SHA256}};
    }
};
inline const World& world() {
    static const World w;
    return w;
}

// ---- the data of a row (EST-R-903) -------------------------------------------------------------------------------------------------------------------------------------------------------------
struct Data {
    std::vector<odl::time::Epoch> t;
    std::vector<Vec3> p_itrs_m;
};
/// Reads the whole of one satellite from SP3 text, or refuses with EST-F-901 naming the epoch (a refusal of the reader itself is passed on under the same identifier).
inline std::optional<std::string> read_whole(const std::string& text, const std::string& sat, Data& out) {
    const World& w = world();
    auto sp3 = odl::io::read_sp3(text);
    if (!sp3) return "EST-F-901: the SP3 reader refused the text: " + std::string(sp3.error().id) + " " + std::string(sp3.error().message);
    if (sp3->header.pos_vel_flag != odl::io::Sp3PosVelFlag::Position) return "EST-F-901: the header flag is not P";
    auto scale = odl::io::to_time_scale(sp3->header.time_system);
    if (!scale) return "EST-F-901: the time system has no scale";
    out = Data{};
    for (std::size_t k = 0; k < sp3->epochs.size(); ++k) {
        auto t = odl::time::Epoch::from_calendar(*scale, sp3->epochs[k].epoch, w.leaps);
        if (!t) return "EST-F-901: epoch " + std::to_string(k) + " has no instant";
        if (k > 0 && std::fabs(t->difference(out.t.back()).to_seconds() - kSpacingS) > 1e-6) return "EST-F-901: epoch " + std::to_string(k) + " is not 900 s after the one before";
        const odl::io::Sp3PositionRecord* rec = nullptr;
        for (const auto& s : sp3->epochs[k].satellites)
            if (s.position.satellite_id == sat) rec = &s.position;
        if (!rec) return "EST-F-901: " + sat + " absent at epoch " + std::to_string(k);
        if (rec->x_km >= 999999.0 || rec->y_km >= 999999.0 || rec->z_km >= 999999.0) return "EST-F-901: " + sat + " is flagged bad at epoch " + std::to_string(k);
        out.t.push_back(*t);
        out.p_itrs_m.push_back(odl::metres_from_km(Vec3{rec->x_km, rec->y_km, rec->z_km}));
    }
    if (out.t.size() != static_cast<std::size_t>(kEpochs)) return "EST-F-901: " + std::to_string(out.t.size()) + " epochs, not 96";
    return std::nullopt;
}
inline Data read_row(const Row& r) {
    Data d;
    const auto refusal = read_whole(slurp(r.file), r.sat, d);
    if (refusal) FAIL(*refusal);
    return d;
}

// ---- the cannonball of a row (EST-Q-011, ruled) -------------------------------------------------------------------------------------------------------------------------------------------------
struct Cannonball {
    double panel_area_m2 = 0, bus_area_m2 = 0, mass_kg = 0;
};
inline Cannonball describe(const Row& r) {
    auto block = must(r.iif ? odl::spacecraft::gps_block_iif() : odl::spacecraft::gps_block_iir_m(), "box-wing macromodel");
    Cannonball c;
    c.mass_kg = block.mass_kg().value();
    for (const auto& s : block.surfaces())
        if (const auto* f = std::get_if<odl::macromodel::FlatSurface>(&s)) (f->normal_mode() == odl::macromodel::NormalMode::sun_pointing ? c.panel_area_m2 : c.bus_area_m2) += f->area_m2().value();
    return c;
}

struct Config {
    bool box_wing = false;
    int degree = 10;
    bool set_b = false;           // the five planets of SPEC-forcemodel FMOD-A-001, added to the Sun and the Moon
    bool desai = false;           // Desai's ocean pole tide in the tides plugin (EST-Q-011: out of set A, printed beside)
    double area_factor = 1.0;     // the cannonball's nominal A*C_R/m times this (EST-A-900b (c))
};

struct Evaluation {
    std::vector<double> r;                 // p - q, 288, unit weight
    std::vector<double> jac;               // dq/dtheta, 288 x 7 row-major (empty when not asked for)
    std::vector<odl::frames::GcrsState> states;
    std::vector<odl::Mat3> to_itrs;        // the GCRS -> ITRS rotation of each epoch
};

class Model {
public:
    const Row& row;
    const Data& d;
    Config cfg;
    double area_m2 = 0, mass_kg = 0;
    dyn::ParameterRegistry registry;
    dyn::ParameterId c_id;
    std::unique_ptr<dyn::ForceSet> set;
    std::string gravity_provenance;

    Model(const Row& rw, const Data& data, const Config& c) : row(rw), d(data), cfg(c), c_id(registry.declare({dyn::ParameterKind::srp_scale, "1", "srp_scale", rw.name})), set(std::make_unique<dyn::ForceSet>(registry)) {
        const World& w = world();
        odl::forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        go.sources = w.sources();
        auto dg = must(odl::gravity::Degree::of(cfg.degree), "degree");
        auto og = must(odl::gravity::Order::of(cfg.degree), "order");
        const auto eo = odl::forcemodel::EarthOrientation::from_series(w.c04, odl::eop::EopPolicy{}, w.leaps);
        add(std::make_shared<odl::forcemodel::Gravity>(w.model, eo, dg, og, go));
        std::vector<odl::eph::Body> bodies{odl::eph::Body::Sun, odl::eph::Body::Moon};
        if (cfg.set_b) for (auto b : {odl::eph::Body::MercuryBarycentre, odl::eph::Body::VenusBarycentre, odl::eph::Body::MarsBarycentre, odl::eph::Body::JupiterBarycentre, odl::eph::Body::SaturnBarycentre}) bodies.push_back(b);
        auto tb = must(odl::forcemodel::third_bodies(bodies, w.ephemeris, w.gm, w.leaps, w.sources()), "third bodies");
        for (auto& f : tb) add(f);
        odl::forcemodel::TidesOptions to;
        to.ocean = &w.fes;
        to.ocean_degree = 7;
        if (cfg.desai) { to.ocean_pole = &w.desai; to.ocean_pole_degree = 10; }
        to.extrapolate_secular = true;
        to.sources = w.sources();
        add(std::make_shared<odl::forcemodel::Tides>(w.model, eo, w.ephemeris, w.leaps, to));
        for (auto term : {odl::relativity::Term::Schwarzschild, odl::relativity::Term::LenseThirring, odl::relativity::Term::DeSitter})
            add(std::make_shared<odl::forcemodel::Relativity>(term, w.ephemeris, w.leaps, odl::relativity::PpnParameters::general_relativity(), w.sources()));
        const Cannonball cb = describe(row);
        area_m2 = cb.panel_area_m2 * cfg.area_factor;
        mass_kg = cb.mass_kg;
        std::shared_ptr<const dyn::Force> srp;
        if (cfg.box_wing) {
            auto block = must(row.iif ? odl::spacecraft::gps_block_iif() : odl::spacecraft::gps_block_iir_m(), "box-wing macromodel");
            srp = std::make_shared<odl::srp::Srp>(block, w.ephemeris, w.leaps);
        } else {
            auto area = must(odl::macromodel::cited(area_m2, "the box-wing macromodel's sun-pointing surfaces' total area (EST-Q-011, odl Committee 2026-11-27)"), "area");
            auto ab = must(odl::macromodel::cited(kCr, "C_R = 1: a perfect absorber (EST-Q-011)"), "absorptivity");
            auto sp = must(odl::macromodel::cited(0.0, "perfect absorber"), "specular");
            auto df = must(odl::macromodel::cited(0.0, "perfect absorber"), "diffuse");
            odl::macromodel::MacromodelBuilder b;
            b.add_surface(odl::macromodel::SphericalSurface{area, ab, sp, df, std::nullopt});
            auto m = must(odl::macromodel::cited(mass_kg, "the block's cited mass (SPEC-spacecraft; PROVENANCE.md 30.2)"), "mass");
            auto com = must(odl::macromodel::cited(Vec3{0.0, 0.0, 0.0}, "cannonball: the centre of mass is the centre"), "centre of mass");
            b.set_mass(m).set_centre_of_mass(com);
            auto mm = must(std::move(b).build(), "macromodel");
            srp = std::make_shared<odl::srp::Srp>(mm, w.ephemeris, w.leaps);
        }
        add(must(odl::forcemodel::ScaledForce::create(srp, c_id, registry), "ScaledForce"));
    }
    void add(std::shared_ptr<dyn::Force> f) { REQUIRE(set->add(std::move(f)).has_value()); }

    [[nodiscard]] double a_cr_over_m() const { return cfg.box_wing ? 0.0 : area_m2 * kCr / mass_kg; }

    static odl::frames::GcrsState state_of(const odl::time::Epoch& t, const double* x) {
        return odl::frames::GcrsState{t, odl::km_from_metres(Vec3{x[0], x[1], x[2]}), odl::km_from_metres(Vec3{x[3], x[4], x[5]})};
    }

    /// The residual (and, when asked, the Jacobian) at theta.  An error is a refusal text.
    [[nodiscard]] std::optional<std::string> evaluate(const Theta& theta, bool jacobian, Evaluation& out) const {
        const World& w = world();
        dyn::ParameterSet params{registry};
        if (!params.set(c_id, theta[6]).has_value()) return "parameter set";
        const auto n = static_cast<std::size_t>(kEpochs);
        std::vector<odl::frames::GcrsState> st(n, state_of(d.t[static_cast<std::size_t>(kMid)], theta.data()));
        std::vector<dyn::Mat6> phi(n, dyn::identity6());
        std::vector<std::array<double, 6>> sens(n, std::array<double, 6>{});
        auto step = [&](std::size_t k, std::size_t from) -> std::optional<std::string> {
            const double secs = d.t[k].difference(d.t[from]).to_seconds();
            if (jacobian) {
                auto s = dyn::propagate_with_sensitivities(*set, params, registry, st[from], secs, kTolerance);
                if (!s) return std::string(s.error().id) + " " + std::string(s.error().message);
                auto col = s->column(registry, c_id);
                if (!col) return "sensitivity column";
                st[k] = s->state;
                dyn::Mat6 np{};
                for (std::size_t i = 0; i < 6; ++i)
                    for (std::size_t j = 0; j < 6; ++j) {
                        double a = 0.0;
                        for (std::size_t l = 0; l < 6; ++l) a += s->phi[i][l] * phi[from][l][j];
                        np[i][j] = a;
                    }
                std::array<double, 6> ns{};
                for (std::size_t i = 0; i < 6; ++i) {
                    double a = (*col)[i];
                    for (std::size_t l = 0; l < 6; ++l) a += s->phi[i][l] * sens[from][l];
                    ns[i] = a;
                }
                phi[k] = np;
                sens[k] = ns;
            } else {
                auto s = dyn::propagate(*set, params, st[from], secs, kTolerance);
                if (!s) return std::string(s.error().id) + " " + std::string(s.error().message);
                st[k] = *s;
            }
            return std::nullopt;
        };
        for (int k = kMid - 1; k >= 0; --k)
            if (auto e = step(static_cast<std::size_t>(k), static_cast<std::size_t>(k) + 1)) return e;
        for (int k = kMid + 1; k < kEpochs; ++k)
            if (auto e = step(static_cast<std::size_t>(k), static_cast<std::size_t>(k) - 1)) return e;
        out.r.clear();
        out.jac.assign(jacobian ? kRows * kNp : 0, 0.0);
        out.to_itrs.clear();
        for (std::size_t k = 0; k < n; ++k) {
            auto rec = w.c04.at(d.t[k], odl::eop::EopPolicy{});
            if (!rec) return "EOP";
            auto rot = odl::frames::gcrs_to_itrs(d.t[k], *rec, w.leaps);
            if (!rot) return "gcrs_to_itrs";
            const Vec3 q = rot->m.apply(odl::metres_from_km(st[k].position()));
            out.r.push_back(d.p_itrs_m[k].x - q.x);
            out.r.push_back(d.p_itrs_m[k].y - q.y);
            out.r.push_back(d.p_itrs_m[k].z - q.z);
            out.to_itrs.push_back(rot->m);
            if (jacobian) {
                for (std::size_t j = 0; j < 7; ++j) {
                    Vec3 col{};
                    if (j < 6) col = Vec3{phi[k][0][j], phi[k][1][j], phi[k][2][j]};
                    else col = odl::metres_from_km(Vec3{sens[k][0], sens[k][1], sens[k][2]});
                    const Vec3 dq = rot->m.apply(col);
                    out.jac[(3 * k + 0) * kNp + j] = dq.x;
                    out.jac[(3 * k + 1) * kNp + j] = dq.y;
                    out.jac[(3 * k + 2) * kNp + j] = dq.z;
                }
            }
        }
        out.states = std::move(st);
        return std::nullopt;
    }

    [[nodiscard]] std::vector<double> residual(const Theta& th) const {
        Evaluation e;
        const auto err = evaluate(th, false, e);
        if (err) FAIL("model: " << *err);
        return e.r;
    }
};

/// SPEC-estimation 3.9: the position of epoch 48 and the velocity of the degree-8 Lagrange polynomial through epochs 44-52 (the nine-point central formula), in the ITRS, carried to the GCRS; srp_scale = 1.
inline Theta nominal_start(const Data& d) {
    const World& w = world();
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
    const auto rec = must(w.c04.at(t, odl::eop::EopPolicy{}), "EOP");
    const auto g = must(odl::frames::to_gcrs(s, rec, w.leaps), "to_gcrs");
    const Vec3 r = odl::metres_from_km(g.position()), vv = odl::metres_from_km(g.velocity());
    return {r.x, r.y, r.z, vv.x, vv.y, vv.z, 1.0};
}

inline const std::array<std::string, kNp>& names() {
    static const std::array<std::string, kNp> n{"x", "y", "z", "vx", "vy", "vz", "srp_scale"};
    return n;
}

/// The LM model over the parameters `free` (the others held at `base`): residual and the Jacobian's free columns.
inline est::LmModel lm_model(const Model& m, const Theta& base, std::vector<std::size_t> free) {
    return [&m, base, free](std::span<const double> x, bool with_jac) -> odl::Result<est::LmEvaluation, est::EstimationRefusal> {
        Theta th = base;
        for (std::size_t i = 0; i < free.size(); ++i) th[free[i]] = x[i];
        Evaluation ev;
        if (const auto err = m.evaluate(th, with_jac, ev)) return odl::err(est::EstimationRefusal{"L7-EXIT-MODEL", "the model refused a point: " + *err});
        est::LmEvaluation out;
        out.residual = std::move(ev.r);
        if (with_jac) {
            out.jacobian.resize(kRows * free.size());
            for (std::size_t k = 0; k < kRows; ++k)
                for (std::size_t i = 0; i < free.size(); ++i) out.jacobian[k * free.size() + i] = ev.jac[k * kNp + free[i]];
        }
        return out;
    };
}

inline double sum_sq(const std::vector<double>& r) {
    double s = 0.0;
    for (double x : r) s += x * x;
    return s;
}
inline double s_n(const std::vector<double>& r) { return std::sqrt(sum_sq(r) / kEpochs); }

/// The nominal start's neighbours for the reported variants (EST-A-900b (c)).
inline Theta perturbed(Theta t) {
    for (std::size_t i = 0; i < 3; ++i) t[i] += 1000.0;
    for (std::size_t i = 3; i < 6; ++i) t[i] += 1.0;
    t[6] = 2.0;
    return t;
}

// ---- a 7x7 symmetric positive definite inverse (Gauss-Jordan with partial pivoting): the test's own, independent of the estimator ---------------------------------------------------------------
inline std::vector<double> invert(std::vector<double> a, std::size_t n) {
    std::vector<double> inv(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i) inv[i * n + i] = 1.0;
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t p = c;
        for (std::size_t r = c + 1; r < n; ++r)
            if (std::fabs(a[r * n + c]) > std::fabs(a[p * n + c])) p = r;
        for (std::size_t j = 0; j < n; ++j) { std::swap(a[c * n + j], a[p * n + j]); std::swap(inv[c * n + j], inv[p * n + j]); }
        const double piv = a[c * n + c];
        REQUIRE(piv != 0.0);
        for (std::size_t j = 0; j < n; ++j) { a[c * n + j] /= piv; inv[c * n + j] /= piv; }
        for (std::size_t r = 0; r < n; ++r) {
            if (r == c) continue;
            const double f = a[r * n + c];
            for (std::size_t j = 0; j < n; ++j) { a[r * n + j] -= f * a[c * n + j]; inv[r * n + j] -= f * inv[c * n + j]; }
        }
    }
    return inv;
}
/// The correlation matrix of the inverse of J^T J, J being rows x n row-major.
inline std::vector<double> correlation_of(const std::vector<double>& jac, std::size_t rows_n, std::size_t n) {
    std::vector<double> nm(n * n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) {
            double s = 0.0;
            for (std::size_t k = 0; k < rows_n; ++k) s += jac[k * n + i] * jac[k * n + j];
            nm[i * n + j] = s;
        }
    const auto q = invert(nm, n);
    std::vector<double> rho(n * n);
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j) rho[i * n + j] = q[i * n + j] / std::sqrt(q[i * n + i] * q[j * n + j]);
    return rho;
}

}  // namespace l7exit
