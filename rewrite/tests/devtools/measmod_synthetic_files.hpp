#pragma once
// tests/devtools/measmod_synthetic_files.hpp -- the texts of the synthetic input files of the measmod generators' tests (plan L0 step 8, group C8): the SINEX estimate block of SLRF2020, the SINEX eccentricity block, the
// Horizons table and the SP3 file that tools/measmod_reference.cpp (and, through its registry section, tools/measmod_fd_sizing.cpp) read.  The shapes are the real files' (columns as the generators read them), the numbers
// are whatever a test puts in.

#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/text.hpp>

#include <string>

namespace odl::devtools_testing {

inline std::string estimate_line(int index, const char* type, const char* code, const char* value_text) {
    return "     " + std::to_string(index) + " " + type + "   " + code + "  A    1 15:001:00000 m    2 " + value_text + " 0.27901E-01\n";
}

/// a SOLUTION/ESTIMATE block for 7090 A 1 whose six values are these, preceded by a line of another pad (which must be ignored)
inline std::string slrf_text(const char* stax, const char* stay, const char* staz, const char* velx, const char* vely, const char* velz) {
    std::string t = "%=SNX 2.02 ILRS\n+SOLUTION/ESTIMATE\n*INDEX TYPE__ CODE PT SOLN _REF_EPOCH__ UNIT S __ESTIMATED VALUE____ _STD_DEV___\n";
    t += estimate_line(1, "STAX", "1181", "0.380062089164670E+07");
    t += estimate_line(2, "STAX", "7090", stax);
    t += estimate_line(3, "STAY", "7090", stay);
    t += estimate_line(4, "STAZ", "7090", staz);
    t += estimate_line(5, "VELX", "7090", velx);
    t += estimate_line(6, "VELY", "7090", vely);
    t += estimate_line(7, "VELZ", "7090", velz);
    t += "-SOLUTION/ESTIMATE\n";
    return t;
}

/// one row of SITE/ECCENTRICITY for 7090 A 1 (the span ends where `end` says: 00:000:00000 is open-ended), the eccentricity in the order up, north, east, and the SOD last
inline std::string ecc_row(const char* end, const char* up, const char* north, const char* east, const char* sod) {
    return std::string(" 7090  A    1 L 14:080:00000 ") + end + " UNE " + up + " " + north + " " + east + "        " + sod + "\n";
}

inline std::string ecc_text(const std::string& rows) {
    return "%=SNX 2.02 ILRS\n+SITE/ECCENTRICITY\n*SITE PT SOLN T DATA_START__ DATA_END____ UNE UP______ NORTH___ EAST____        CDP-SOD_\n" + rows + "-SITE/ECCENTRICITY\n";
}

inline std::string horizons_text(const std::string& vector_lines) {
    return " X = 99 Y = 99 Z = 99 (before the marker: not read)\n*******\n$$SOE\n2461234.5 = A.D. 2026-Sep-25 00:00:00.0000 TDB\n" + vector_lines + "$$EOE\n";
}

inline std::string sp3_text(const std::string& records) {
    return "#dP2026  1  3  0  0  0.00000000      96 d+D  IGS14 FIT  ILR\n## 2402 345600.00000000   900.00000000 61043 0.0000000000000\n+    1   L51\n*  2026  1  3  0  0  0.00000000\n" + records;
}

inline std::string sp3_record(double x, double y, double z) {
    return "PL51" + odl::devkit::pad_left(odl::devkit::py_format_f(x, 6), 14) + odl::devkit::pad_left(odl::devkit::py_format_f(y, 6), 14) + odl::devkit::pad_left(odl::devkit::py_format_f(z, 6), 14) +
           "     999999.999999\n";
}

}  // namespace odl::devtools_testing
