#pragma once
// tools/measmod_registry_facts.hpp — the registry's data facts, counted independently of the registry (SPEC-measmod.md MEAS-A-004 / MEAS-A-010; plan L0 step 8, group C8).
//
// `run` is the whole tool: `measmod_registry_facts [--root DIR] [--check]`.  Exit 0 printed (and, with --check, every pinned number matched), 1 a pinned number did not match, 2 an argument error or an input
// file that is missing, cannot be read or is not the shape expected, 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace odl::tools::measmod_registry_facts {

/// An input that is missing, unreadable or not the shape this reads (the Python's OSError, IndexError, ValueError): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// where the three pinned files are, relative to the tree's root
inline constexpr char kSlrfRelative[] = "data/cache/ilrs-slrf2020-20260205/SLRF2020_POS+VEL_2026.02.05.snx";
inline constexpr char kEccRelative[] = "data/cache/ilrs-slrecc-une-20260527/slrecc.260527.ILRS.une.snx";
inline constexpr char kPsdRelative[] = "data/cache/itrf2020-psd-slr/ITRF2020-psd-slr.dat";

/// Days from 1970-01-01 to a date of the proleptic Gregorian calendar (Howard Hinnant's days_from_civil); the month is 1 to 12.
[[nodiscard]] std::int64_t days_from_civil(std::int64_t year, int month, int day);

/// A SINEX epoch YY:DOY:SSSSS (a field of a split line: no whitespace in it) as seconds since 1970-01-01 (Python: datetime(y, 1, 1) + timedelta(days=doy - 1, seconds=sec), the two-digit year read as 2000 + yy
/// below 50 and 1900 + yy above; with `end`, the seconds 86399 mean the end of the day, the start of the next); none for 00:000:xxxxx, the open end or the unknown start.  Throws BadInput for what is not
/// an epoch and for one no datetime holds (the years 1 to 9999: where the Python raised ValueError or OverflowError).
[[nodiscard]] std::optional<std::int64_t> snx_epoch(const std::string& e, bool end);

/// The numbers SPEC-measmod.md states for the pinned release (MEAS-A-004, MEAS-A-006, MEAS-R-006, §9.1), in the order the tool prints its counts.
[[nodiscard]] const std::vector<std::pair<std::string, std::int64_t>>& expected();

/// What the three files say: the counts, in print order, the post-seismic events of each site (in file order) and the SODs of the eccentricity file that are on a pad SLRF2020 lists and are not placed (sorted).
struct Facts {
    std::vector<std::pair<std::string, std::int64_t>> counts;
    std::vector<std::pair<std::string, std::vector<std::string>>> events;
    std::vector<std::string> unplaced_on_known_pads;
};

/// The facts of the three files' lines (each the file's text split into lines).  Reads nothing else.
[[nodiscard]] Facts count(const std::vector<std::string>& slrf, const std::vector<std::string>& ecc, const std::vector<std::string>& psd);

struct Settings {
    std::filesystem::path root;
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::measmod_registry_facts
