// odl_campaign_gnss.cpp — L8 step 1: the GNSS campaigns against the IGS final orbits (plan/subplan_L8/L8-1.md, registered 2026-11-27; rulings L8-R1 and L8-R2 of odl Committee).
//
//   odl_campaign_gnss --mode baselines --sp3-022 FILE --sp3-023 FILE  <world>              G-01, G-02, G-03 (cannonball, EST-Q-011's block-referenced template) against the L7 exit gate's minima
//   odl_campaign_gnss --mode blocks    --sp3 FILE --metadata FILE     <world> [--sat Gnn]   B-*: every satellite of one day's file fitted with the BLIND template (A = 1 m^2, m = 50 kg, C_R = 1), block means of A C_R / m
//   odl_campaign_gnss --mode ecom      --sp3 FILE                     <world> [--sat Gnn]   E-*: every satellite fitted with ECOM2 d4b1_order() (15 parameters)
//   <world> = --leap F --de440s F --gm F --egm2008 F --fes2004 F --c04 F --cache-root DIR   (every one required; no path is built in)
//   [--threads N]   per-satellite workers, default 1; EACH WORKER OWNS ITS OWN GnssWorld (the CALCEPH handle is not thread-safe) and the rows are written in PRN order whatever the completion order
//   [--out FILE]    the report is also written to FILE.partial and renamed to FILE
//   Statistics are per single-day file (L8-R2), never pooled over days; each is printed with the formula it was computed with.
//
//   exit 0: every criterion met   1: a campaign criterion missed, or an error   2: a usage error or a refused input
#include <odl/harness/gnss_campaign.hpp>
#include <odl/harness/igs_metadata.hpp>
#include <odl/harness/statistics.hpp>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace h = odl::harness;

namespace {

struct Args {
    std::string mode, sp3, sp3_022, sp3_023, metadata, sat, out;
    h::GnssWorldPaths world;
    std::size_t threads = 1;
};

[[noreturn]] void usage(const std::string& why) {
    std::fprintf(stderr, "odl_campaign_gnss: %s\nusage: odl_campaign_gnss --mode baselines|blocks|ecom (--sp3 FILE | --sp3-022 FILE --sp3-023 FILE) [--metadata FILE] --leap F --de440s F --gm F --egm2008 F --fes2004 F --c04 F --cache-root DIR [--sat Gnn] [--threads N] [--out FILE]\n", why.c_str());
    std::exit(2);
}

Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        const auto val = [&]() -> std::string {
            if (i + 1 >= argc) usage("missing the value of " + k);
            return argv[++i];
        };
        if (k == "--mode") a.mode = val();
        else if (k == "--sp3") a.sp3 = val();
        else if (k == "--sp3-022") a.sp3_022 = val();
        else if (k == "--sp3-023") a.sp3_023 = val();
        else if (k == "--metadata") a.metadata = val();
        else if (k == "--leap") a.world.leap_seconds = val();
        else if (k == "--de440s") a.world.de440s = val();
        else if (k == "--gm") a.world.gm = val();
        else if (k == "--egm2008") a.world.egm2008 = val();
        else if (k == "--fes2004") a.world.fes2004 = val();
        else if (k == "--c04") a.world.c04 = val();
        else if (k == "--cache-root") a.world.cache_root = val();
        else if (k == "--sat") a.sat = val();
        else if (k == "--out") a.out = val();
        else if (k == "--threads") {
            const long n = std::strtol(val().c_str(), nullptr, 10);
            if (n < 1) usage("--threads needs an integer >= 1");
            a.threads = static_cast<std::size_t>(n);
        } else usage("unknown argument " + k);
    }
    if (a.mode != "baselines" && a.mode != "blocks" && a.mode != "ecom") usage("--mode must be baselines, blocks or ecom");
    for (const auto& [name, v] : {std::pair<const char*, const std::string&>{"--leap", a.world.leap_seconds}, {"--de440s", a.world.de440s}, {"--gm", a.world.gm}, {"--egm2008", a.world.egm2008},
                                  {"--fes2004", a.world.fes2004}, {"--c04", a.world.c04}, {"--cache-root", a.world.cache_root}})
        if (v.empty()) usage(std::string(name) + " is required");
    if (a.mode == "baselines" && (a.sp3_022.empty() || a.sp3_023.empty())) usage("--mode baselines needs --sp3-022 and --sp3-023");
    if (a.mode != "baselines" && a.sp3.empty()) usage("--mode " + a.mode + " needs --sp3");
    if (a.mode == "blocks" && a.metadata.empty()) usage("--mode blocks needs --metadata");
    return a;
}

std::optional<std::string> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::nullopt;
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

struct Report {
    std::string text;
    int exit_code = 0;
    void line(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
};
void Report::line(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    text += buf;
    text += '\n';
}

std::string fit_line(const std::string& label, const h::GnssFitResult& f) {
    char buf[1024];
    std::snprintf(buf, sizeof buf, "%s status=%s passes=%zu S_N=%.10g m S_{N-p}=%.10g m S_N/sqrt(3)=%.10g m S_N(start)/S_N=%.6g min_nu=%.17g A*C_R/m=%.9g m^2/kg srp_scale=%.12g", label.c_str(), f.status.c_str(), f.passes, f.stats.s_n_m,
                  f.stats.s_n_minus_p_m, f.stats.s_n_over_sqrt3_m, f.start_over_end(), f.min_shadow_nu, f.a_cr_over_m, f.srp_scale);
    return buf;
}

/// Runs `job(world, index)` for every index in [0, n) on `threads` workers, each owning a GnssWorld; results in index order.
template <class R, class Job>
std::vector<R> run_parallel(const h::GnssWorldPaths& paths, std::size_t n, std::size_t threads, std::string& error, Job job) {
    std::vector<R> results(n);
    std::atomic<std::size_t> next{0};
    std::vector<std::string> errors(std::min(threads, std::max<std::size_t>(n, 1)));
    std::vector<std::thread> pool;
    for (std::size_t t = 0; t < errors.size(); ++t)
        pool.emplace_back([&, t] {
            auto world = h::GnssWorld::load(paths);
            if (!world) {
                errors[t] = std::string(world.error().id) + " " + world.error().message;
                return;
            }
            for (std::size_t i = next.fetch_add(1); i < n; i = next.fetch_add(1)) results[i] = job(**world, i);
        });
    for (auto& th : pool) th.join();
    for (const auto& e : errors)
        if (!e.empty()) error = e;
    return results;
}

struct Row {
    std::string prn, svn, block;
    std::optional<h::GnssFitResult> fit;
    std::string error;
};

int mode_baselines(const Args& a, Report& r) {
    struct Case { const char* name; const std::string* file; const char* sat; bool iif; double minimum; double F; };
    const Case cases[3] = {{"G-01", &a.sp3_022, "G01", true, 0.0622784085, 0.0645}, {"G-02", &a.sp3_023, "G01", true, 0.0696950657, 0.0573}, {"G-03", &a.sp3_022, "G05", false, 0.242196639, 0.1870}};
    r.line("odl_campaign_gnss --mode baselines: the L7 exit gate's cannonball fits through odl::harness (gravity degree 10, set A, EST-Q-011 block-referenced templates)");
    std::string error;
    struct Out { std::string text; bool met = false; bool ok = false; };
    auto outs = run_parallel<Out>(a.world, 3, a.threads, error, [&](const h::GnssWorld& w, std::size_t i) {
        Out o;
        const Case& c = cases[i];
        const auto text = slurp(*c.file);
        if (!text) { o.text = std::string(c.name) + " ERROR cannot read " + *c.file; return o; }
        auto arc = h::read_sp3_arc(w, *text, c.sat);
        if (!arc) { o.text = std::string(c.name) + " ERROR " + std::string(arc.error().id) + " " + arc.error().message; return o; }
        auto tpl = h::block_referenced_template(c.iif);
        if (!tpl) { o.text = std::string(c.name) + " ERROR " + tpl.error().message; return o; }
        auto fit = h::fit_gnss_cannonball(w, *arc, *tpl);
        if (!fit) { o.text = std::string(c.name) + " ERROR " + std::string(fit.error().id) + " " + fit.error().message; return o; }
        o.ok = true;
        const bool rel = std::fabs(fit->stats.s_n_m - c.minimum) <= 1e-6 * c.minimum, within = fit->stats.s_n_m <= 2.0 * c.F;
        o.met = fit->converged && rel && within && fit->start_over_end() >= 10.0;
        char buf[512];
        std::snprintf(buf, sizeof buf, "  template A=%.6g m^2 m=%.6g kg C_R=%.3g; recorded minimum %.10g m (relative difference %.3g, <= 1e-6: %s); S_N <= 2F = %.4f m: %s -> %s", tpl->area_m2, tpl->mass_kg, tpl->cr, c.minimum,
                      std::fabs(fit->stats.s_n_m - c.minimum) / c.minimum, rel ? "yes" : "NO", 2.0 * c.F, within ? "yes" : "NO", o.met ? "MET" : "MISSED");
        o.text = std::string(c.name) + " " + fit_line("", *fit) + "\n" + buf;
        return o;
    });
    if (!error.empty()) { std::fprintf(stderr, "odl_campaign_gnss: %s\n", error.c_str()); return 2; }
    bool all = true;
    for (const Out& o : outs) {
        r.text += o.text + "\n";
        all = all && o.met;
    }
    r.line("baselines: %s", all ? "every criterion met" : "a criterion MISSED or a fit failed");
    return all ? 0 : 1;
}

int mode_sweep(const Args& a, Report& r, bool ecom) {
    const auto sp3 = slurp(a.sp3);
    if (!sp3) { std::fprintf(stderr, "odl_campaign_gnss: cannot read %s\n", a.sp3.c_str()); return 2; }
    auto prns = h::list_sp3_prns(*sp3);
    if (!prns) { std::fprintf(stderr, "odl_campaign_gnss: %s %s\n", std::string(prns.error().id).c_str(), prns.error().message.c_str()); return 2; }
    std::vector<std::string> list;
    for (const auto& p : *prns)
        if (a.sat.empty() || p == a.sat) list.push_back(p);
    if (list.empty()) { std::fprintf(stderr, "odl_campaign_gnss: satellite %s is not in %s\n", a.sat.c_str(), a.sp3.c_str()); return 2; }
    std::optional<h::IgsSatelliteMetadata> meta;
    if (!ecom) {
        const auto sx = slurp(a.metadata);
        if (!sx) { std::fprintf(stderr, "odl_campaign_gnss: cannot read %s\n", a.metadata.c_str()); return 2; }
        auto m = h::IgsSatelliteMetadata::parse(*sx);
        if (!m) { std::fprintf(stderr, "odl_campaign_gnss: %s %s\n", std::string(m.error().id).c_str(), m.error().message.c_str()); return 2; }
        meta = std::move(*m);
    }
    r.line("odl_campaign_gnss --mode %s: %zu satellite(s) of %s, one single-day file (L8-R2), %zu worker(s)", ecom ? "ecom" : "blocks", list.size(), a.sp3.c_str(), a.threads);
    if (ecom) r.line("model: ECOM2 d4b1_order(), 15 parameters [x y z vx vy vz D0 D2c D2s D4c D4s Y0 B0 B1c B1s]");
    else r.line("model: the blind cannonball template of L8-R1 (A = 1 m^2, m = 50 kg, C_R = 1: A*C_R/m = 0.02 m^2/kg), 7 parameters; A*C_R/m = srp_scale * 0.02");
    std::string error;
    auto rows = run_parallel<Row>(a.world, list.size(), a.threads, error, [&](const h::GnssWorld& w, std::size_t i) {
        Row row;
        row.prn = list[i];
        auto arc = h::read_sp3_arc(w, *sp3, row.prn);
        if (!arc) { row.error = std::string(arc.error().id) + " " + arc.error().message; return row; }
        if (meta) {
            auto e = meta->resolve_prn(row.prn, h::SinexEpoch::from_calendar(arc->first_calendar));
            if (!e) { row.error = std::string(e.error().id) + " " + e.error().message; return row; }
            row.svn = e->svn;
            row.block = e->block;
        }
        auto fit = ecom ? h::fit_gnss_ecom2(w, *arc) : h::fit_gnss_cannonball(w, *arc, h::blind_template());
        if (!fit) { row.error = std::string(fit.error().id) + " " + fit.error().message; return row; }
        row.fit = std::move(*fit);
        return row;
    });
    if (!error.empty()) { std::fprintf(stderr, "odl_campaign_gnss: %s\n", error.c_str()); return 2; }
    bool ok = true;
    std::vector<double> sn;
    std::vector<bool> free_of_shadow;
    std::map<std::string, std::vector<double>> per_block;
    for (const Row& row : rows) {                                   // PRN order: list is sorted
        if (!row.fit) {
            r.line("%s %s %s ERROR %s", row.prn.c_str(), row.block.c_str(), row.svn.c_str(), row.error.c_str());
            ok = false;
            continue;
        }
        r.line("%s", fit_line(row.prn + (row.block.empty() ? "" : " " + row.block + " " + row.svn), *row.fit).c_str());
        if (!row.fit->converged || row.fit->start_over_end() < 10.0) ok = false;
        sn.push_back(row.fit->stats.s_n_m);
        free_of_shadow.push_back(row.fit->min_shadow_nu >= 1.0);
        if (!ecom) per_block[row.block].push_back(row.fit->a_cr_over_m);
    }
    r.line("criterion: every fit converged with S_N(start)/S_N(end) >= 10: %s", ok ? "MET" : "MISSED");
    if (ecom) {
        std::vector<double> clear;
        for (std::size_t i = 0; i < sn.size(); ++i)
            if (free_of_shadow[i]) clear.push_back(sn[i]);
        auto all = h::compute_constellation_summary(sn, free_of_shadow);
        if (all) r.line("all %zu satellites (%zu eclipse-free): median(S_N) = %.10g m [E-01 formula: median over the satellites, the mean of the two middle values for an even count], max(S_N) = %.10g m [E-02: the largest S_N]", all->count, all->eclipse_free_count, all->median_sn_m, all->max_sn_m);
        if (!clear.empty()) {
            const std::vector<bool> flags(clear.size(), true);
            auto cs = h::compute_constellation_summary(clear, flags);
            if (cs) {
                const bool e1 = cs->median_sn_m <= 2.0 * 0.0364, e2 = cs->max_sn_m <= 2.0 * 0.0442;
                r.line("eclipse-free (min nu = 1) %zu satellites: median(S_N) = %.10g m <= 2 x 0.0364 = 0.0728 m: %s; max(S_N) = %.10g m <= 2 x 0.0442 = 0.0884 m: %s", cs->count, cs->median_sn_m, e1 ? "MET" : "MISSED", cs->max_sn_m, e2 ? "MET" : "MISSED");
                if (!e1 || !e2) ok = false;
            }
        } else {
            r.line("eclipse-free subset: no satellite with min nu = 1; the gross-error bounds are not evaluated");
        }
    } else {
        std::vector<h::BlockSummary> sums;
        for (const auto& [block, vals] : per_block) {
            auto s = h::compute_block_summary(block, vals);
            if (!s) { r.line("block %s: n_b = %zu: %s", block.c_str(), vals.size(), s.error().message.c_str()); continue; }
            r.line("block %s: n_b = %zu, m_b = %.9g m^2/kg, s_b = %.9g m^2/kg [m_b = mean, s_b = Bessel sd]", block.c_str(), s->n, s->mean, s->sd);
            sums.push_back(*s);
        }
        const auto find = [&](const char* b) -> const h::BlockSummary* {
            for (const auto& s : sums)
                if (s.block == b) return &s;
            return nullptr;
        };
        const h::BlockSummary* iif = find("GPS-IIF");
        for (const char* other : {"GPS-IIR-A", "GPS-IIR-M", "GPS-IIIA"}) {
            const h::BlockSummary* o = find(other);
            if (!iif || !o) continue;
            auto sep = h::compute_block_separation(*iif, *o);
            if (sep) r.line("separation GPS-IIF vs %s: d_pooled = %.4g [|m1-m2| / sqrt(((n1-1)s1^2+(n2-1)s2^2)/(n1+n2-2))], t_sem = %.4g [|m1-m2| / sqrt(s1^2/n1+s2^2/n2)]   (frozen B-*: IIF-IIR-A 14.1 / 20.8, IIF-IIIA 9.8 / 11.3; a different epoch)", other, sep->d_pooled, sep->t_sem);
        }
        r.line("ordering of the block means (the frozen B-*: IIF > IIR-B > IIR-A > IIR-M > IIIA; a prediction, not a criterion): reported above");
    }
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    const Args a = parse(argc, argv);
    Report r;
    const int code = a.mode == "baselines" ? mode_baselines(a, r) : mode_sweep(a, r, a.mode == "ecom");
    if (code == 2) return 2;
    r.line("exit %d", code);
    std::fputs(r.text.c_str(), stdout);
    if (!a.out.empty()) {
        const std::string tmp = a.out + ".partial";
        {
            std::ofstream f(tmp, std::ios::binary);
            if (!f) { std::fprintf(stderr, "odl_campaign_gnss: cannot write %s\n", tmp.c_str()); return 2; }
            f << r.text;
        }
        if (std::rename(tmp.c_str(), a.out.c_str()) != 0) { std::fprintf(stderr, "odl_campaign_gnss: cannot rename %s\n", tmp.c_str()); return 2; }
    }
    return code;
}
