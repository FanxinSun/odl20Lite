#pragma once
// nist_nls.hpp — the fixture of SPEC-estimation.md §8.3 (L7 step 3): the NIST StRD NONLINEAR regression datasets as the problems a fit is run on.  The files are read from the
// PINNED CACHE (the manifest's `nist-strd-nls-<name>` entries); nothing of NIST's is copied into the tree.  The 27 models are written HERE from the model line of each file (the
// listing of the 27 is in REPORT.files, step3_models), as templates over the number type, and the Jacobian is the forward-mode derivative of the very same code (dual.hpp).
// "log[y]" (Nelson) is fitted as the file states it: the response is the logarithm of the data column, the model is b1 - b2 x1 exp(-b3 x2).
#include <odl/estimation/levenberg_marquardt.hpp>

#include "dual.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef ODL_MANIFEST_CACHE_ROOT
#error "ODL_MANIFEST_CACHE_ROOT must name the manifest's cache directory (the module's CMakeLists defines it)"
#endif

namespace odl::estimation::fx {

using ld = long double;

struct NlsData {
    std::string name;
    std::size_t n = 0;                               ///< parameters
    std::size_t m = 0;                               ///< observations
    std::size_t columns = 0;                         ///< predictor columns (1; Nelson has 2)
    std::vector<double> y;                           ///< the response as fitted (log for Nelson)
    std::vector<double> x;                           ///< m x columns row-major
    std::vector<double> start1, start2;
    std::vector<ld> certified, certified_sd;
    ld certified_rss = 0.0L;                         ///< NIST's residual sum of squares
    std::string difficulty;
};

inline const std::vector<std::string>& nls_names() {
    static const std::vector<std::string> v = {"Bennett5", "BoxBOD", "Chwirut1", "Chwirut2", "DanWood", "Eckerle4", "ENSO", "Gauss1", "Gauss2", "Gauss3", "Hahn1", "Kirby2", "Lanczos1", "Lanczos2",
                                               "Lanczos3", "MGH09", "MGH10", "MGH17", "Misra1a", "Misra1b", "Misra1c", "Misra1d", "Nelson", "Rat42", "Rat43", "Roszman1", "Thurber"};
    return v;
}

[[nodiscard]] inline std::string nls_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// The model value (T = double) or value and gradient (T = Dual) at predictors `x` (`columns` of them) for parameters `b` (1-based in the files, 0-based here).
template <class T>
T nls_model(const std::string& name, const T* b, const double* x) {
    const double X = x[0];
    if (name == "Bennett5") return b[0] * pow(b[1] + X, -1.0 / b[2]);
    if (name == "BoxBOD" || name == "Misra1a") return b[0] * (1.0 - exp(-b[1] * X));
    if (name == "Chwirut1" || name == "Chwirut2") return exp(-b[0] * X) / (b[1] + b[2] * X);
    if (name == "DanWood") return b[0] * pow(T(X), b[1]);
    if (name == "Eckerle4") {
        const T z = (X - b[2]) / b[1];
        return (b[0] / b[1]) * exp(-0.5 * z * z);
    }
    if (name == "ENSO") {
        const double pi = 3.141592653589793238462643383279;
        return b[0] + b[1] * cos(2.0 * pi * X / 12.0) + b[2] * sin(2.0 * pi * X / 12.0) + b[4] * cos(2.0 * pi * X / b[3]) + b[5] * sin(2.0 * pi * X / b[3]) +
               b[7] * cos(2.0 * pi * X / b[6]) + b[8] * sin(2.0 * pi * X / b[6]);
    }
    if (name == "Gauss1" || name == "Gauss2" || name == "Gauss3") {
        const T u = (X - b[3]) / b[4];
        const T w = (X - b[6]) / b[7];
        return b[0] * exp(-b[1] * X) + b[2] * exp(-(u * u)) + b[5] * exp(-(w * w));
    }
    if (name == "Hahn1" || name == "Thurber")
        return (b[0] + b[1] * X + b[2] * X * X + b[3] * X * X * X) / (1.0 + b[4] * X + b[5] * X * X + b[6] * X * X * X);
    if (name == "Kirby2") return (b[0] + b[1] * X + b[2] * X * X) / (1.0 + b[3] * X + b[4] * X * X);
    if (name == "Lanczos1" || name == "Lanczos2" || name == "Lanczos3") return b[0] * exp(-b[1] * X) + b[2] * exp(-b[3] * X) + b[4] * exp(-b[5] * X);
    if (name == "MGH09") return b[0] * (X * X + X * b[1]) / (X * X + X * b[2] + b[3]);
    if (name == "MGH10") return b[0] * exp(b[1] / (X + b[2]));
    if (name == "MGH17") return b[0] + b[1] * exp(-X * b[3]) + b[2] * exp(-X * b[4]);
    if (name == "Misra1b") {
        const T q = 1.0 + b[1] * X / 2.0;
        return b[0] * (1.0 - 1.0 / (q * q));
    }
    if (name == "Misra1c") return b[0] * (1.0 - pow(1.0 + 2.0 * b[1] * X, -0.5));
    if (name == "Misra1d") return b[0] * b[1] * X / (1.0 + b[1] * X);
    if (name == "Nelson") return b[0] - b[1] * x[0] * exp(-b[2] * x[1]);
    if (name == "Rat42") return b[0] / (1.0 + exp(b[1] - b[2] * X));
    if (name == "Rat43") return b[0] / pow(1.0 + exp(b[1] - b[2] * X), 1.0 / b[3]);
    if (name == "Roszman1") {
        const double pi = 3.141592653589793238462643383279;
        return b[0] - b[1] * X - atan(b[2] / (X - b[3])) / pi;
    }
    throw std::runtime_error("no model named " + name);
}

[[nodiscard]] inline NlsData load_nls(const std::string& name) {
    const std::string path = std::string(ODL_MANIFEST_CACHE_ROOT) + "/nist-strd-nls-" + nls_lower(name) + "/" + name + ".dat";
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    NlsData d;
    d.name = name;
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(in, line)) lines.push_back(line);
    // the table of starts and certified values, the residual sum of squares, the counts
    std::size_t first_data = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        if (l.find("Level of Difficulty") != std::string::npos && d.difficulty.empty()) {
            const std::size_t a = l.find_first_not_of(' ');
            d.difficulty = l.substr(a, l.find(" Level") - a);
        }
        int idx = 0;
        char s1[64], s2[64], c[64], sd[64];
        if (std::sscanf(l.c_str(), " b%d = %63s %63s %63s %63s", &idx, s1, s2, c, sd) == 5 && idx >= 1) {
            if (d.start1.size() < static_cast<std::size_t>(idx)) {
                d.start1.resize(static_cast<std::size_t>(idx));
                d.start2.resize(static_cast<std::size_t>(idx));
                d.certified.resize(static_cast<std::size_t>(idx));
                d.certified_sd.resize(static_cast<std::size_t>(idx));
            }
            d.start1[static_cast<std::size_t>(idx) - 1] = std::strtod(s1, nullptr);
            d.start2[static_cast<std::size_t>(idx) - 1] = std::strtod(s2, nullptr);
            d.certified[static_cast<std::size_t>(idx) - 1] = std::strtold(c, nullptr);
            d.certified_sd[static_cast<std::size_t>(idx) - 1] = std::strtold(sd, nullptr);
        }
        if (l.rfind("Residual Sum of Squares:", 0) == 0) d.certified_rss = std::strtold(l.c_str() + 24, nullptr);
        if (l.rfind("Data:", 0) == 0 && l.find("Response") == std::string::npos && l.find("y ") != std::string::npos && i > 20) first_data = i + 1;
    }
    d.n = d.start1.size();
    // the data: every following line of numbers, first column the response
    for (std::size_t i = first_data; i < lines.size(); ++i) {
        std::istringstream ss(lines[i]);
        std::vector<double> row;
        std::string tok;
        while (ss >> tok) row.push_back(std::strtod(tok.c_str(), nullptr));
        if (row.size() < 2) continue;
        if (d.columns == 0) d.columns = row.size() - 1;
        if (row.size() != d.columns + 1) throw std::runtime_error(name + ": a data line with " + std::to_string(row.size()) + " numbers");
        d.y.push_back(row[0]);
        for (std::size_t c = 1; c < row.size(); ++c) d.x.push_back(row[c]);
    }
    d.m = d.y.size();
    if (name == "Nelson")
        for (double& v : d.y) v = std::log(v);          // the file's model is log[y] = ...
    return d;
}

/// The model of a dataset as the callback of the fit: whitened with sigma = 1, residual = observed - computed, Jacobian = the partials of the model.
[[nodiscard]] inline LmModel nls_problem(const NlsData& d) {
    return [&d](std::span<const double> b, bool with_jacobian) -> odl::Result<LmEvaluation, EstimationRefusal> {
        LmEvaluation e;
        e.residual.resize(d.m);
        if (with_jacobian) e.jacobian.assign(d.m * d.n, 0.0);
        std::vector<Dual> bd(d.n);
        for (std::size_t i = 0; i < d.n; ++i) bd[i] = Dual::variable(b[i], i);
        for (std::size_t k = 0; k < d.m; ++k) {
            const double* xk = &d.x[k * d.columns];
            if (with_jacobian) {
                const Dual f = nls_model<Dual>(d.name, bd.data(), xk);
                e.residual[k] = d.y[k] - f.v;
                for (std::size_t i = 0; i < d.n; ++i) e.jacobian[k * d.n + i] = f.g[i];
            } else {
                e.residual[k] = d.y[k] - nls_model<double>(d.name, b.data(), xk);
            }
        }
        return e;
    };
}

/// The log relative error of NIST: -log10(|b - c| / |c|) per parameter, the minimum; 11 for a value correct to 11 figures (capped at 15).
[[nodiscard]] inline double lre_min(const std::vector<double>& b, const std::vector<ld>& c) {
    double worst = 15.0;
    for (std::size_t i = 0; i < c.size(); ++i) {
        const ld err = std::fabs(static_cast<ld>(b[i]) - c[i]);
        const double lre = err == 0.0L ? 15.0 : -std::log10(static_cast<double>(err / std::fabs(c[i])));
        worst = std::min(worst, std::min(15.0, lre));
    }
    return worst;
}

/// F = (1/2) sum r^2 of a dataset at b, from the double model.
[[nodiscard]] inline double nls_objective(const NlsData& d, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t k = 0; k < d.m; ++k) {
        const double r = d.y[k] - nls_model<double>(d.name, b.data(), &d.x[k * d.columns]);
        s += r * r;
    }
    return 0.5 * s;
}

}  // namespace odl::estimation::fx
