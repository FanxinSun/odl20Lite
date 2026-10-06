#pragma once
// odl/devkit/pyfmt.hpp — the number formats Python printed and the committed artefacts hold.
//
// `%.17g`, `%.17e`, `%g` and the rest of the printf family are glibc's and need nothing here: both languages print the exactly rounded decimal
// expansion.  Two formats are Python's own and are written here from their definitions: repr(float), the SHORTEST digit string that reads back
// as the same double laid out by Python's rules, and float.hex().

#include <string>

namespace odl::devkit {

/// repr(float): "1.0", "0.0001", "1e-05", "1e+16", "1.2345678901234568e+17", "inf", "-inf", "nan".  Fixed notation when the decimal point
/// falls in -4 < decpt <= 16 (decpt = digits before the point), exponent notation otherwise with at least two exponent digits.
[[nodiscard]] std::string py_float_repr(double v);

/// float.hex(): "0x1.8000000000000p+0" (always thirteen hexadecimal digits), "0x0.0p+0" for zero, a subnormal as "0x0.xxxxxxxxxxxxxp-1022".
[[nodiscard]] std::string py_float_hex(double v);

}  // namespace odl::devkit
