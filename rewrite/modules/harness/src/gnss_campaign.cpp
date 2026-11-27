#include <odl/harness/gnss_campaign.hpp>

#include <odl/core/units.hpp>
#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/ecom/ecom.hpp>
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
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <variant>

namespace odl::harness {

namespace dyn = odl::dyn;
namespace est = odl::estimation;

namespace {

constexpr std::size_t kEpochs = 96;
constexpr std::size_t kMid = 48;
constexpr double kSpacingS = 900.0;
constexpr double kTolerance = 1e-12;            // the integrator tolerance of every model call and of the variational equations
constexpr double kBadPosition = 999999.0;       // an SP3 position component at or above this is the bad/absent marker (clock fields do not decide)

[[nodiscard]] HarnessError refusal(std::string_view id, std::string text) { return HarnessError{id, std::move(text)}; }

template <class E>
[[nodiscard]] HarnessError passed_on(std::string_view id, const char* what, const E& e) {
    return HarnessError{id, std::string(what) + ": " + std::string(e.id) + " " + std::string(e.message)};
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
struct GnssWorld::Impl {
    std::optional<odl::time::LeapTable> leaps;
    std::optional<odl::eph::Ephemeris> ephemeris;
    std::optional<odl::thirdbody::GravitationalParameters> gm;
    std::optional<odl::gravity::GravityModel> model;
    std::optional<odl::eop::EopSeries> c04;
    std::optional<odl::tides::OceanTide> fes;
};

GnssWorld::GnssWorld() : impl_(std::make_unique<Impl>()) {}
GnssWorld::~GnssWorld() = default;

namespace {

[[nodiscard]] odl::Result<std::string, HarnessError> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return odl::err(refusal("HARN-F-020", "cannot read " + path));
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

}  // namespace

odl::Result<std::unique_ptr<GnssWorld>, HarnessError> GnssWorld::load(const GnssWorldPaths& p) {
    std::unique_ptr<GnssWorld> w(new GnssWorld());
    Impl& m = *w->impl_;
    auto leap_text = slurp(p.leap_seconds);
    if (!leap_text) return odl::err(leap_text.error());
    auto leaps = odl::time::LeapTable::parse(*leap_text, odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    if (!leaps) return odl::err(passed_on("HARN-F-020", "leap seconds", leaps.error()));
    m.leaps = std::move(*leaps);
    auto eph = odl::eph::Ephemeris::open({p.de440s}, {});
    if (!eph) return odl::err(passed_on("HARN-F-020", "ephemeris", eph.error()));
    m.ephemeris = std::move(*eph);
    auto gm = odl::thirdbody::GravitationalParameters::load(p.gm, p.cache_root);
    if (!gm) return odl::err(passed_on("HARN-F-020", "gravitational parameters", gm.error()));
    m.gm = std::move(*gm);
    auto model = odl::gravity::GravityModel::load(p.egm2008, p.cache_root, odl::gravity::ScalingParameters::egm2008_tt_compatible());
    if (!model) return odl::err(passed_on("HARN-F-020", "EGM2008", model.error()));
    m.model = std::move(*model);
    auto c04_text = slurp(p.c04);
    if (!c04_text) return odl::err(c04_text.error());
    auto c04 = odl::eop::EopSeries::load_c04(*c04_text, odl::eop::EopProvenance{"eop-c04-20", "", "", ""}, *m.leaps);
    if (!c04) return odl::err(passed_on("HARN-F-020", "EOP C04", c04.error()));
    m.c04 = std::move(*c04);
    auto fes = odl::tides::OceanTide::load(p.fes2004, p.cache_root);
    if (!fes) return odl::err(passed_on("HARN-F-020", "FES2004", fes.error()));
    m.fes = std::move(*fes);
    return w;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
odl::Result<Sp3Arc, HarnessError> read_sp3_arc(const GnssWorld& world, std::string_view text, const std::string& satellite) {
    const GnssWorld::Impl& w = world.impl();
    auto sp3 = odl::io::read_sp3(text);
    if (!sp3) return odl::err(refusal("EST-F-901", "the SP3 reader refused the text: " + std::string(sp3.error().id) + " " + std::string(sp3.error().message)));
    if (sp3->header.pos_vel_flag != odl::io::Sp3PosVelFlag::Position) return odl::err(refusal("EST-F-901", "the header flag is not P"));
    auto scale = odl::io::to_time_scale(sp3->header.time_system);
    if (!scale) return odl::err(refusal("EST-F-901", "the time system has no scale"));
    Sp3Arc arc;
    arc.satellite = satellite;
    for (std::size_t k = 0; k < sp3->epochs.size(); ++k) {
        auto t = odl::time::Epoch::from_calendar(*scale, sp3->epochs[k].epoch, *w.leaps);
        if (!t) return odl::err(refusal("EST-F-901", "epoch " + std::to_string(k) + " has no instant"));
        if (k > 0 && std::fabs(t->difference(arc.epochs.back()).to_seconds() - kSpacingS) > 1e-6) return odl::err(refusal("EST-F-901", "epoch " + std::to_string(k) + " is not 900 s after the one before"));
        const odl::io::Sp3PositionRecord* rec = nullptr;
        for (const auto& s : sp3->epochs[k].satellites)
            if (s.position.satellite_id == satellite) rec = &s.position;
        if (!rec) return odl::err(refusal("EST-F-901", satellite + " absent at epoch " + std::to_string(k)));
        if (rec->x_km >= kBadPosition || rec->y_km >= kBadPosition || rec->z_km >= kBadPosition) return odl::err(refusal("EST-F-901", satellite + " is flagged bad at epoch " + std::to_string(k)));
        if (rec->clock_us >= kBadPosition) ++arc.clock_absent_records;
        if (k == 0) arc.first_calendar = sp3->epochs[0].epoch;
        arc.epochs.push_back(*t);
        arc.p_itrs_m.push_back(odl::metres_from_km(Vec3{rec->x_km, rec->y_km, rec->z_km}));
    }
    if (arc.epochs.size() != kEpochs) return odl::err(refusal("EST-F-901", std::to_string(arc.epochs.size()) + " epochs, not 96"));
    return arc;
}

odl::Result<std::vector<std::string>, HarnessError> list_sp3_prns(std::string_view text) {
    auto sp3 = odl::io::read_sp3(text);
    if (!sp3) return odl::err(refusal("EST-F-901", "the SP3 reader refused the text: " + std::string(sp3.error().id) + " " + std::string(sp3.error().message)));
    std::vector<std::string> ids = sp3->header.satellite_ids;
    std::sort(ids.begin(), ids.end());
    return ids;
}

CannonballTemplate blind_template() noexcept { return CannonballTemplate{1.0, 50.0, 1.0}; }

odl::Result<CannonballTemplate, HarnessError> block_referenced_template(bool block_iif) {
    auto block = block_iif ? odl::spacecraft::gps_block_iif() : odl::spacecraft::gps_block_iir_m();
    if (!block) return odl::err(passed_on("HARN-F-021", "box-wing macromodel", block.error()));
    CannonballTemplate t;
    t.area_m2 = 0.0;
    t.mass_kg = block->mass_kg().value();
    t.cr = 1.0;
    for (const auto& s : block->surfaces())
        if (const auto* f = std::get_if<odl::macromodel::FlatSurface>(&s))
            if (f->normal_mode() == odl::macromodel::NormalMode::sun_pointing) t.area_m2 += f->area_m2().value();
    return t;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
namespace {

enum class Kind { cannonball, box_wing, ecom2 };

struct Bundle {
    dyn::ParameterRegistry registry;
    std::vector<dyn::ParameterId> pids;        // the force parameters, in the order of theta[6..]
    std::unique_ptr<dyn::ForceSet> set;
};

struct Evaluation {
    std::vector<double> r;                     // p - q, unit weight, 3 per epoch
    std::vector<double> jac;                   // dq/dtheta, rows x params, row-major (empty when not asked for)
    std::vector<odl::frames::GcrsState> states;
    std::vector<odl::Mat3> gcrs_to_itrs;
};

[[nodiscard]] odl::frames::GcrsState state_of(const odl::time::Epoch& t, const double* x) {
    return odl::frames::GcrsState{t, odl::km_from_metres(Vec3{x[0], x[1], x[2]}), odl::km_from_metres(Vec3{x[3], x[4], x[5]})};
}

template <class T>
[[nodiscard]] bool add_force(dyn::ForceSet& set, std::shared_ptr<T> f, std::string& err) {
    auto r = set.add(std::move(f));
    if (!r) {
        err = std::string(r.error().id) + " " + std::string(r.error().message);
        return false;
    }
    return true;
}

[[nodiscard]] odl::Result<std::unique_ptr<Bundle>, HarnessError> build_bundle(const GnssWorld::Impl& w, Kind kind, const CannonballTemplate& tpl, bool iif, int degree) {
    auto b = std::make_unique<Bundle>();
    // the force parameters are declared FIRST (the registry must be complete before the set sees it)
    std::optional<odl::ecom::EcomParameterIds> ecom_ids;
    const dyn::ParameterId first_id = kind == Kind::ecom2 ? b->registry.declare({dyn::ParameterKind::empirical_acceleration, "m/s^2", "D0", "satellite"})
                                                          : b->registry.declare({dyn::ParameterKind::srp_scale, "1", "srp_scale", "satellite"});
    if (kind == Kind::ecom2) {
        const odl::ecom::EcomOrder order = odl::ecom::d4b1_order();
        odl::ecom::EcomParameterIds ids{first_id, {}, {}, first_id, first_id, {}, {}};
        const auto decl = [&](const char* name) { return b->registry.declare({dyn::ParameterKind::empirical_acceleration, "m/s^2", name, "satellite"}); };
        static const char* const dc[] = {"D2c", "D4c", "D6c"};
        static const char* const ds[] = {"D2s", "D4s", "D6s"};
        static const char* const bc[] = {"B1c", "B3c", "B5c"};
        static const char* const bs[] = {"B1s", "B3s", "B5s"};
        for (int i = 0; i < order.n_D; ++i) {
            ids.D_even_c.push_back(decl(dc[i]));
            ids.D_even_s.push_back(decl(ds[i]));
        }
        ids.Y0 = decl("Y0");
        ids.B0 = decl("B0");
        for (int i = 0; i < order.n_B; ++i) {
            ids.B_odd_c.push_back(decl(bc[i]));
            ids.B_odd_s.push_back(decl(bs[i]));
        }
        ecom_ids = ids;
        b->pids = ids.flattened();
    } else {
        b->pids = {first_id};
    }
    b->set = std::make_unique<dyn::ForceSet>(b->registry);
    std::string err;
    odl::forcemodel::GravityOptions go;
    go.extrapolate_secular = true;
    auto dg = odl::gravity::Degree::of(degree);
    auto og = odl::gravity::Order::of(degree);
    if (!dg || !og) return odl::err(refusal("HARN-F-022", "the gravity degree is not valid"));
    const auto eo = odl::forcemodel::EarthOrientation::from_series(*w.c04, odl::eop::EopPolicy{}, *w.leaps);
    if (!add_force(*b->set, std::make_shared<odl::forcemodel::Gravity>(*w.model, eo, *dg, *og, go), err)) return odl::err(refusal("HARN-F-022", "Gravity: " + err));
    const std::vector<odl::eph::Body> bodies{odl::eph::Body::Sun, odl::eph::Body::Moon};
    auto tb = odl::forcemodel::third_bodies(bodies, *w.ephemeris, *w.gm, *w.leaps);
    if (!tb) return odl::err(passed_on("HARN-F-022", "third bodies", tb.error()));
    for (auto& f : *tb)
        if (!add_force(*b->set, f, err)) return odl::err(refusal("HARN-F-022", "third body: " + err));
    odl::forcemodel::TidesOptions to;
    to.ocean = &*w.fes;
    to.ocean_degree = 7;                        // TN36-6 Table 6.1's value at GPS
    to.extrapolate_secular = true;
    if (!add_force(*b->set, std::make_shared<odl::forcemodel::Tides>(*w.model, eo, *w.ephemeris, *w.leaps, to), err)) return odl::err(refusal("HARN-F-022", "Tides: " + err));
    for (auto term : {odl::relativity::Term::Schwarzschild, odl::relativity::Term::LenseThirring, odl::relativity::Term::DeSitter})
        if (!add_force(*b->set, std::make_shared<odl::forcemodel::Relativity>(term, *w.ephemeris, *w.leaps, odl::relativity::PpnParameters::general_relativity()), err)) return odl::err(refusal("HARN-F-022", "Relativity: " + err));
    if (kind == Kind::ecom2) {
        if (!add_force(*b->set, std::make_shared<odl::ecom::Ecom>(odl::ecom::d4b1_order(), *ecom_ids, *w.ephemeris, *w.leaps), err)) return odl::err(refusal("HARN-F-022", "Ecom: " + err));
        return b;
    }
    std::shared_ptr<const dyn::Force> srp;
    if (kind == Kind::box_wing) {
        auto block = iif ? odl::spacecraft::gps_block_iif() : odl::spacecraft::gps_block_iir_m();
        if (!block) return odl::err(passed_on("HARN-F-021", "box-wing macromodel", block.error()));
        srp = std::make_shared<odl::srp::Srp>(*block, *w.ephemeris, *w.leaps);
    } else {
        auto area = odl::macromodel::cited(tpl.area_m2, "the cannonball template's area (L8-R1 / EST-Q-011)");
        auto ab = odl::macromodel::cited(tpl.cr, "the cannonball template's C_R: a perfect absorber");
        auto sp = odl::macromodel::cited(0.0, "perfect absorber");
        auto df = odl::macromodel::cited(0.0, "perfect absorber");
        auto mass = odl::macromodel::cited(tpl.mass_kg, "the cannonball template's mass (L8-R1 / EST-Q-011)");
        auto com = odl::macromodel::cited(Vec3{0.0, 0.0, 0.0}, "cannonball: the centre of mass is the centre");
        if (!area || !ab || !sp || !df || !mass || !com) return odl::err(refusal("HARN-F-021", "the cannonball template has an invalid value"));
        odl::macromodel::MacromodelBuilder mb;
        mb.add_surface(odl::macromodel::SphericalSurface{*area, *ab, *sp, *df, std::nullopt});
        mb.set_mass(*mass).set_centre_of_mass(*com);
        auto mm = std::move(mb).build();
        if (!mm) return odl::err(passed_on("HARN-F-021", "cannonball macromodel", mm.error()));
        srp = std::make_shared<odl::srp::Srp>(*mm, *w.ephemeris, *w.leaps);
    }
    auto scaled = odl::forcemodel::ScaledForce::create(srp, b->pids[0], b->registry);
    if (!scaled) return odl::err(passed_on("HARN-F-022", "ScaledForce", scaled.error()));
    if (!add_force(*b->set, *scaled, err)) return odl::err(refusal("HARN-F-022", "ScaledForce: " + err));
    return b;
}

/// The residual (and, when asked, the Jacobian) at theta = [state(6), force parameters]; an empty string is success.
[[nodiscard]] std::string evaluate(const GnssWorld::Impl& w, const Bundle& b, const Sp3Arc& arc, const std::vector<double>& theta, bool jacobian, Evaluation& out) {
    const std::size_t nf = b.pids.size(), np = 6 + nf;
    dyn::ParameterSet params{b.registry};
    for (std::size_t i = 0; i < nf; ++i)
        if (!params.set(b.pids[i], theta[6 + i]).has_value()) return "parameter set";
    std::vector<odl::frames::GcrsState> st(kEpochs, state_of(arc.epochs[kMid], theta.data()));
    std::vector<dyn::Mat6> phi(kEpochs, dyn::identity6());
    std::vector<std::vector<std::array<double, 6>>> sens(kEpochs, std::vector<std::array<double, 6>>(nf, std::array<double, 6>{}));
    const auto step = [&](std::size_t k, std::size_t from) -> std::string {
        const double secs = arc.epochs[k].difference(arc.epochs[from]).to_seconds();
        if (!jacobian) {
            auto s = dyn::propagate(*b.set, params, st[from], secs, kTolerance);
            if (!s) return std::string(s.error().id) + " " + std::string(s.error().message);
            st[k] = *s;
            return {};
        }
        auto s = dyn::propagate_with_sensitivities(*b.set, params, b.registry, st[from], secs, kTolerance);
        if (!s) return std::string(s.error().id) + " " + std::string(s.error().message);
        st[k] = s->state;
        dyn::Mat6 nphi{};
        for (std::size_t i = 0; i < 6; ++i)
            for (std::size_t j = 0; j < 6; ++j) {
                double a = 0.0;
                for (std::size_t l = 0; l < 6; ++l) a += s->phi[i][l] * phi[from][l][j];
                nphi[i][j] = a;
            }
        phi[k] = nphi;
        for (std::size_t f = 0; f < nf; ++f) {
            auto col = s->column(b.registry, b.pids[f]);
            if (!col) return "sensitivity column";
            for (std::size_t i = 0; i < 6; ++i) {
                double a = (*col)[i];
                for (std::size_t l = 0; l < 6; ++l) a += s->phi[i][l] * sens[from][f][l];
                sens[k][f][i] = a;
            }
        }
        return {};
    };
    for (std::size_t k = kMid; k-- > 0;)
        if (auto e = step(k, k + 1); !e.empty()) return e;
    for (std::size_t k = kMid + 1; k < kEpochs; ++k)
        if (auto e = step(k, k - 1); !e.empty()) return e;
    out.r.clear();
    out.jac.assign(jacobian ? 3 * kEpochs * np : 0, 0.0);
    out.gcrs_to_itrs.clear();
    for (std::size_t k = 0; k < kEpochs; ++k) {
        auto rec = w.c04->at(arc.epochs[k], odl::eop::EopPolicy{});
        if (!rec) return "EOP at epoch " + std::to_string(k);
        auto rot = odl::frames::gcrs_to_itrs(arc.epochs[k], *rec, *w.leaps);
        if (!rot) return "gcrs_to_itrs at epoch " + std::to_string(k);
        const Vec3 q = rot->m.apply(odl::metres_from_km(st[k].position()));
        out.r.push_back(arc.p_itrs_m[k].x - q.x);
        out.r.push_back(arc.p_itrs_m[k].y - q.y);
        out.r.push_back(arc.p_itrs_m[k].z - q.z);
        out.gcrs_to_itrs.push_back(rot->m);
        if (!jacobian) continue;
        for (std::size_t j = 0; j < np; ++j) {
            Vec3 col{};
            if (j < 6) col = Vec3{phi[k][0][j], phi[k][1][j], phi[k][2][j]};
            else col = odl::metres_from_km(Vec3{sens[k][j - 6][0], sens[k][j - 6][1], sens[k][j - 6][2]});
            const Vec3 dq = rot->m.apply(col);
            out.jac[(3 * k + 0) * np + j] = dq.x;
            out.jac[(3 * k + 1) * np + j] = dq.y;
            out.jac[(3 * k + 2) * np + j] = dq.z;
        }
    }
    out.states = std::move(st);
    return {};
}

/// The position of the middle epoch and the velocity of the degree-8 Lagrange polynomial through epochs 44 - 52 (the nine-point central formula), ITRS -> GCRS.
[[nodiscard]] odl::Result<std::array<double, 6>, HarnessError> nominal_start(const GnssWorld::Impl& w, const Sp3Arc& arc) {
    static const double wgt[9] = {1.0 / 280.0, -4.0 / 105.0, 1.0 / 5.0, -4.0 / 5.0, 0.0, 4.0 / 5.0, -1.0 / 5.0, 4.0 / 105.0, -1.0 / 280.0};
    Vec3 v{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < 9; ++i) {
        const Vec3& p = arc.p_itrs_m[kMid - 4 + i];
        v.x += wgt[i] * p.x;
        v.y += wgt[i] * p.y;
        v.z += wgt[i] * p.z;
    }
    v = (1.0 / kSpacingS) * v;
    const odl::frames::ItrsState s{arc.epochs[kMid], odl::km_from_metres(arc.p_itrs_m[kMid]), odl::km_from_metres(v)};
    auto rec = w.c04->at(arc.epochs[kMid], odl::eop::EopPolicy{});
    if (!rec) return odl::err(passed_on("HARN-F-023", "EOP at the middle epoch", rec.error()));
    auto g = odl::frames::to_gcrs(s, *rec, *w.leaps);
    if (!g) return odl::err(passed_on("HARN-F-023", "to_gcrs", g.error()));
    const Vec3 r = odl::metres_from_km(g->position()), vv = odl::metres_from_km(g->velocity());
    return std::array<double, 6>{r.x, r.y, r.z, vv.x, vv.y, vv.z};
}

[[nodiscard]] double sum_sq(const std::vector<double>& r) {
    double s = 0.0;
    for (double x : r) s += x * x;
    return s;
}

[[nodiscard]] odl::Result<GnssFitResult, HarnessError> run_fit(const GnssWorld& world, const Sp3Arc& arc, Kind kind, const CannonballTemplate& tpl, bool iif, const FitOptions& options) {
    const GnssWorld::Impl& w = world.impl();
    if (arc.epochs.size() != kEpochs) return odl::err(refusal("EST-F-901", "the arc does not hold 96 epochs"));
    auto bundle = build_bundle(w, kind, tpl, iif, options.gravity_degree);
    if (!bundle) return odl::err(bundle.error());
    const Bundle& b = **bundle;
    auto start = nominal_start(w, arc);
    if (!start) return odl::err(start.error());
    const std::size_t nf = b.pids.size(), np = 6 + nf;
    std::vector<double> theta0(np, 0.0);
    for (std::size_t i = 0; i < 6; ++i) theta0[i] = (*start)[i];
    if (kind != Kind::ecom2) theta0[6] = 1.0;                       // srp_scale = 1 (the template's own A*C_R/m); the ECOM coefficients start at zero
    std::vector<std::string> names{"x", "y", "z", "vx", "vy", "vz"};
    if (kind == Kind::ecom2) {
        for (const char* n : {"D0", "D2c", "D2s", "D4c", "D4s", "Y0", "B0", "B1c", "B1s"}) names.emplace_back(n);
    } else {
        names.emplace_back("srp_scale");
    }

    Evaluation e0;
    if (auto err = evaluate(w, b, arc, theta0, false, e0); !err.empty()) return odl::err(refusal("HARN-F-024", "the model refused the starting point: " + err));
    const double sn_start = std::sqrt(sum_sq(e0.r) / static_cast<double>(kEpochs));

    const est::LmModel model = [&](std::span<const double> x, bool with_jac) -> odl::Result<est::LmEvaluation, est::EstimationRefusal> {
        const std::vector<double> th(x.begin(), x.end());
        Evaluation ev;
        if (auto err = evaluate(w, b, arc, th, with_jac, ev); !err.empty()) return odl::err(est::EstimationRefusal{"L8-HARNESS-MODEL", "the model refused a point: " + err});
        est::LmEvaluation out;
        out.residual = std::move(ev.r);
        out.jacobian = std::move(ev.jac);
        return out;
    };
    est::LmOptions lo;
    lo.max_iterations = options.max_iterations;
    const est::LmResult lm = est::levenberg_marquardt(names, theta0, model, lo);

    Evaluation eh;
    if (auto err = evaluate(w, b, arc, lm.x, false, eh); !err.empty()) return odl::err(refusal("HARN-F-024", "the model refused the solution: " + err));
    std::vector<EpochResidual> eps;
    for (std::size_t k = 0; k < kEpochs; ++k) {
        EpochResidual e;
        e.q_minus_p_itrs_m = Vec3{-eh.r[3 * k], -eh.r[3 * k + 1], -eh.r[3 * k + 2]};
        e.itrs_to_gcrs = eh.gcrs_to_itrs[k].transpose();
        e.position_gcrs_m = odl::metres_from_km(eh.states[k].position());
        e.velocity_gcrs_m = odl::metres_from_km(eh.states[k].velocity());
        eps.push_back(e);
    }
    auto stats = compute_orbit_residuals(eps, np);
    if (!stats) return odl::err(stats.error());
    GnssFitResult r;
    r.model = kind == Kind::cannonball ? "cannonball" : (kind == Kind::box_wing ? "box-wing" : "ecom2-d4b1");
    r.status = est::to_string(lm.status);
    r.converged = lm.status == est::LmStatus::converged_gradient || lm.status == est::LmStatus::converged_residual || lm.status == est::LmStatus::converged_step;
    r.message = lm.message;
    r.passes = lm.iterations;
    r.accepted = lm.accepted_steps;
    r.rejected = lm.rejected_steps;
    r.parameter_names = names;
    r.theta_start = theta0;
    r.theta_hat = lm.x;
    r.s_n_start_m = sn_start;
    r.stats = *stats;
    if (kind != Kind::ecom2) {
        r.srp_scale = lm.x[6];
        if (kind == Kind::cannonball) r.a_cr_over_m = lm.x[6] * tpl.a_cr_over_m();
    }
    for (std::size_t k = 0; k < kEpochs; ++k) {
        auto sun = w.ephemeris->geocentric_state(odl::eph::Body::Sun, arc.epochs[k], *w.leaps);
        if (!sun) return odl::err(passed_on("HARN-F-025", "Sun", sun.error()));
        auto nu = odl::shadow::conical(odl::frames::Position<odl::frames::Frame::GCRS>{odl::metres_from_km(sun->position())}, odl::frames::Position<odl::frames::Frame::GCRS>{odl::metres_from_km(eh.states[k].position())});
        if (!nu) return odl::err(passed_on("HARN-F-025", "shadow function", nu.error()));
        r.min_shadow_nu = std::min(r.min_shadow_nu, nu->fraction);
    }
    return r;
}

}  // namespace

odl::Result<GnssFitResult, HarnessError> fit_gnss_cannonball(const GnssWorld& world, const Sp3Arc& arc, const CannonballTemplate& tpl, const FitOptions& options) {
    return run_fit(world, arc, Kind::cannonball, tpl, true, options);
}
odl::Result<GnssFitResult, HarnessError> fit_gnss_box_wing(const GnssWorld& world, const Sp3Arc& arc, bool block_iif, const FitOptions& options) {
    return run_fit(world, arc, Kind::box_wing, blind_template(), block_iif, options);
}
odl::Result<GnssFitResult, HarnessError> fit_gnss_ecom2(const GnssWorld& world, const Sp3Arc& arc, const FitOptions& options) {
    return run_fit(world, arc, Kind::ecom2, blind_template(), true, options);
}

}  // namespace odl::harness
