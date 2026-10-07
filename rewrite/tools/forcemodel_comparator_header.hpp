#pragma once
// tools/forcemodel_comparator_header.hpp — the data the finite-difference gate's EXTENDED-PRECISION comparator needs (SPEC-forcemodel.md §6.2 (e); plan L0 step 8, group C7): the harmonic polynomials of degrees
// 2 - 4, exactly, and the constants of the registered truncation bound.
//
// `run` is the whole tool: `forcemodel_comparator_header [--header PATH] [--verify PATH]`.  Exit 0 emitted / the committed header is what the generator emits, 1 they differ (or a coefficient is not exact in a
// double), 2 an argument error or a file that cannot be read or written, 70 an error the tool did not anticipate.

#include "gradient_reference.hpp"

#include <odl/devkit/tool.hpp>

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace odl::tools::forcemodel_comparator_header {

/// The highest degree of the harmonics the header carries.
inline constexpr int kNmax = 4;

/// Thrown when a monomial's coefficient is not exact in a double (the Python's assert): the message names the coefficient.
struct InexactCoefficient : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// float(x).hex() of an exact rational: the hexadecimal form of the double that IS the value.  Throws InexactCoefficient for a value no double holds.
[[nodiscard]] std::string hexf_exact(const odl::tools::gradient_reference::Rational& x);

/// The text of the generated header, for the degrees 2 .. nmax; `solid` makes a coefficient's polynomial (the definition's, unless a test hands in another).
[[nodiscard]] std::string build(int nmax = kNmax, const std::function<odl::tools::gradient_reference::Poly(int, int, char)>& solid = odl::tools::gradient_reference::solid_polynomial);

/// The tool.  `argv` is its arguments WITHOUT the program name.  `run_on` makes the polynomials with `solid` (a test hands in a damaged one); `run` is `run_on` the definition's.
[[nodiscard]] int run_on(const std::function<odl::tools::gradient_reference::Poly(int, int, char)>& solid, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::forcemodel_comparator_header
