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

/// format(v, ".{precision}g") (and f"{v:g}", which is precision 6): the printf `%.*g`, which Python's 'g' is for every finite value (both round the exact binary value
/// correctly, drop trailing zeros and write at least two exponent digits), and Python's own words for the rest: "inf", "-inf" and "nan" (printf's `-nan` is not Python's).
[[nodiscard]] std::string py_format_g(double v, int precision = 6);

/// format(v, ".{precision}e") and format(v, ".{precision}f"): the printf `%.*e` and `%.*f`, which Python's 'e' and 'f' are for every finite value (the exactly rounded decimal expansion, at least two exponent digits), and
/// Python's own words for the rest ("inf", "-inf", "nan").  Group C8: the sizing tools print their numbers in these two forms.
[[nodiscard]] std::string py_format_e(double v, int precision);
[[nodiscard]] std::string py_format_f(double v, int precision);

}  // namespace odl::devkit
