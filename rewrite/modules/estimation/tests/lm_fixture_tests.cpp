// lm_fixture_tests.cpp — SPEC-estimation.md §8.3, the checks of the FIXTURE itself, before any fit: that the 27 models written in nist_nls.hpp are the models of the files, and that the
// forward-mode Jacobian is the derivative of that very code.  (Registered as EST-A-300; run before the comparisons of EST-A-301, and not themselves a comparison with anything the fit produces.)
//   (a) the counts the file declares ("Number of Observations", the parameters of its table) are the counts parsed;
//   (b) at the CERTIFIED parameters the residual sum of squares of the written model equals NIST's certified residual sum of squares: a published case, so a transcription error in a model,
//       a column or the log of Nelson's response cannot pass (relative tolerance 1e-6: the certified parameters are rounded to 11 figures, and the sums are of up to 250 terms);
//   (c) the dual-number Jacobian agrees with a central difference of the double model at start 1 and at the certified point, column by column, to 1e-6 relative to the column's norm.
#include "nist_nls.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace odl::estimation::fx;

TEST_CASE("EST-A-300: the 27 NIST nonlinear models are the files' models, and their forward-mode Jacobians are their derivatives", "[estimation][lm][fixture]") {
    CHECK(nls_names().size() == 27);
    for (const std::string& nm : nls_names()) {
        const NlsData d = load_nls(nm);
        INFO(nm);
        // (a)
        std::ifstream in(std::string(ODL_MANIFEST_CACHE_ROOT) + "/nist-strd-nls-" + nls_lower(nm) + "/" + nm + ".dat");
        std::string line;
        std::size_t declared_m = 0;
        while (std::getline(in, line))
            if (line.rfind("Number of Observations:", 0) == 0) declared_m = std::strtoul(line.c_str() + 23, nullptr, 10);
        CHECK(d.m == declared_m);
        CHECK(d.n >= 2);
        CHECK(d.start2.size() == d.n);
        CHECK(d.certified.size() == d.n);
        // (b)
        std::vector<double> cert(d.n);
        for (std::size_t i = 0; i < d.n; ++i) cert[i] = static_cast<double>(d.certified[i]);
        const double rss = 2.0 * nls_objective(d, cert);
        const double nist = static_cast<double>(d.certified_rss);
        // datasets whose certified residual sum is below the rounding of the certified PARAMETERS (11 figures) cannot be reproduced relatively: an absolute floor, 1e-18, for them
        CHECK(std::fabs(rss - nist) <= 1.0e-6 * nist + (nist < 1.0e-20 ? 1.0e-18 : 0.0));
        // (c)
        const auto model = nls_problem(d);
        for (const std::vector<double>* b : std::initializer_list<const std::vector<double>*>{&d.start1, &cert}) {
            const auto e = model(*b, true);
            REQUIRE(e.has_value());
            for (std::size_t i = 0; i < d.n; ++i) {
                const double h = 6.0e-6 * std::max(std::fabs((*b)[i]), 1.0e-8);
                std::vector<double> bp = *b, bm = *b;
                bp[i] += h;
                bm[i] -= h;
                double diff2 = 0.0, norm2 = 0.0;
                double fmax = 0.0;
                for (std::size_t k = 0; k < d.m; ++k) {
                    const double fd = (nls_model<double>(nm, bp.data(), &d.x[k * d.columns]) - nls_model<double>(nm, bm.data(), &d.x[k * d.columns])) / (2.0 * h);
                    const double an = e->jacobian[k * d.n + i];
                    fmax = std::max(fmax, std::fabs(nls_model<double>(nm, bp.data(), &d.x[k * d.columns])));
                    diff2 += (fd - an) * (fd - an);
                    norm2 += an * an;
                }
                INFO("parameter " << i << " of " << (b == &d.start1 ? "start 1" : "the certified point"));
                // a central difference has a rounding floor of its own, 4 u fmax sqrt(m) / h (the column may be dwarfed by a constant term of the model, as MGH17's b1 does)
                const double floor = 4.0 * 1.1102230246251565e-16 * fmax * std::sqrt(static_cast<double>(d.m)) / h;
                CHECK(std::sqrt(diff2) <= 1.0e-6 * std::sqrt(norm2) + floor);
            }
        }
    }
}
