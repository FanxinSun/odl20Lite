#pragma once
// tools/legendre_reference.hpp — reference values for GRAV-A-003, from the DEFINITION (plan L0 step 8, group C7).
//
// `run` is the whole tool: `legendre_reference [--header PATH] [--verify PATH]`.  Exit 0 emitted / the committed header is what the generator emits, 1 a value failed its two-precision agreement check (or the
// committed header differs), 2 an argument error or a file that cannot be read or written, 70 an error the tool did not anticipate.
//
// The pieces: the integer coefficients of the definition's sum, the sum evaluated at one precision, the two-precision guard, and the tool.  Everything is a function of its arguments, so that a test hands the
// tool fewer points, or a precision too low for the cancellation, and shows it REFUSE.

#include <odl/devkit/bigint.hpp>
#include <odl/devkit/decimal.hpp>
#include <odl/devkit/rational.hpp>
#include <odl/devkit/tool.hpp>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace odl::tools::legendre_reference {

using odl::devkit::BigInt;
using odl::devkit::Decimal;
using odl::devkit::Rational;

/// N_nm^2 from TN36-6 (6.2b), exactly.
[[nodiscard]] Rational norm_squared(int n, int m);

/// The integers coeff_j = (-1)^j C(n,j) C(2n-2j,n) (n-2j)!/(n-2j-m)!, j = 0, 1, ... while n - 2j - m >= 0 (none when m > n): the coefficients of u^(n-2j-m) in 2^n P_n^(m)(u).
[[nodiscard]] std::vector<BigInt> coefficients(int n, int m);

/// Pbar'_nm(u) = N_nm 2^-n sum_j coeff_j u^(n-2j-m), in `precision` digits, for u given as the text of a decimal number (exact).  The sum is evaluated by Horner's rule in u^2.
[[nodiscard]] Decimal pbar_factored(int n, int m, std::string_view u, int precision);

/// Thrown by value: its message is the Python's ValueError text.
struct RefusedError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The precisions of the two evaluations: lo = per_degree * n + base digits, hi = lo + gap.
struct Precisions {
    int per_degree = 4;
    int base = 400;
    int gap = 300;
};

/// Pbar'_nm(u) at 60 digits, evaluated twice at different precisions, and refused (RefusedError) if the two disagree by more than 1e-50.
[[nodiscard]] Decimal value(int n, int m, std::string_view u, const Precisions& precisions = {});

/// What the tool computes.
struct Settings {
    std::vector<std::pair<int, int>> points;   ///< (n, m)
    std::vector<std::string> sines;            ///< sin(phi), as decimal text
    Precisions precisions;
};
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` does the work of `settings`; `run` is `run_on` the real ones.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::legendre_reference
