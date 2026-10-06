#pragma once
// gfz_derive.hpp — the derivative of GFZ's Kp/ap/F10.7 file that the tree vendors, and the checks that say it is one.
//
// WHY THERE IS A DERIVATIVE.  The upstream file (kp.gfz-potsdam.de, Kp_ap_Ap_SN_F107_since_1932.txt) is CC BY 4.0 EXCEPT its sunspot-number column
// SN, which is CC BY-NC 4.0 (WDC-SILSO, Royal Observatory of Belgium; the file's own header, lines 2 and 18).  The tree redistributes (vendors) the
// bytes its pin names, because upstream rewrites the file daily and a clean clone could not otherwise fetch them; it may not redistribute
// non-commercial data (its allowlist refuses CC-BY-NC-4.0), and it consumes none of that column (ATMO-R-032).  So the vendored file is the pinned
// upstream file with ONE change, stated byte-exactly:
//
//     in every data row (the lines after the file's 40 '#' header lines), the four bytes at 0-based offsets 134..137 -- the SN field, a separating
//     blank and a three-character right-aligned integer -- are replaced by "  -1", the file's own marker for a missing SN ("missing data
//     indicated by ... -1 for ap and SN", its header line 30).  Every other byte, the 40 header lines included, is unchanged.
//
// The change is adaptation of CC BY 4.0 material, which that licence allows with attribution and an indication of the change: the manifest entry
// and NOTICE carry both.  `derive` performs it; `check_derived` is what the vendored file must satisfy WITHOUT the original (which only exists
// where somebody fetched it on 2026-09-18, since upstream no longer serves those bytes); the executable's three forms are in gfz_derive.cpp.

#include <odl/devkit/tool.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odl::tools::gfz_derive {

/// The upstream file the manifest pinned on 2026-09-18 (34 594 daily rows, 1932-01-01 .. 2026-09-17), and the derivative the tree vendors in its place.
inline constexpr std::string_view kOriginalSha256 = "0121821b20799731f00cccc3d1393789d8cca136b5f3cfbae9c464cc029e3a73";
inline constexpr std::string_view kDerivedSha256 = "3f67ee8dc9d6fa60ebf4f00803d013c0ba00f369427a7a493a15539f4804c7ad";

/// The layout, from the file's own header ("The format for each line is ...").
inline constexpr std::size_t kHeaderLines = 40;           // all starting with '#'
inline constexpr std::size_t kRowChars = 158;             // a data row without its line feed
inline constexpr std::size_t kSnOffset = 134;             // 0-based offset of the SN field in a row
inline constexpr std::size_t kSnWidth = 4;
inline constexpr std::string_view kSnMarker = "  -1";     // the field as "%4d" prints -1

struct Derived {
    std::string bytes;
    std::size_t rows = 0;      // data rows
    std::size_t changed = 0;   // rows whose SN field was not already the marker
};

/// Replaces the SN field of every data row by the marker; nothing else changes.  `original` must be shaped as the file is (header lines, fixed-width
/// rows, a number or the marker in the field): anything else is refused with std::runtime_error, in words that name the line.
[[nodiscard]] Derived derive(std::string_view original);

/// The first way `text` fails to be the derivative this tree vendors, or nullopt: the shape as for `derive`; every SN field the marker; every other
/// field parsed in its own columns (the separators blank; the date agreeing with the day number, the day numbers consecutive; Kp, ap, Ap, F10.7, D in
/// their ranges).  It needs no original.
[[nodiscard]] std::optional<std::string> check_derived(std::string_view text);

/// The two hashes a run is held to: the input must be the pinned original, the output the pinned derivative.
struct Pins {
    std::string_view original;
    std::string_view derived;
};

/// The tool.  `argv` is its arguments WITHOUT the program name:  <original> <derived>  |  --check <original> <derived>  |  --verify <derived>.
/// 0 ok, 2 the derivative differs from what was derived / fails its checks, 3 an input refused, 5 usage error, 70 an error not anticipated.
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

/// The same with other pins: the tests' own synthetic files hash to nothing the tool knows.
[[nodiscard]] int run_with(const std::vector<std::string>& argv, odl::devkit::Streams io, const Pins& pins);

}  // namespace odl::tools::gfz_derive
