#pragma once
// tools/measmod_g5_select.hpp — the G5 pass-selection rule of SPEC-measmod.md §8.9, applied to the normal-point file's METADATA (plan L0 step 8, group C8).
//
// `run` is the whole tool: `measmod_g5_select [--check] [--root DIR]`.  Exit 0 printed (and, with --check, the choice reproduced), 1 the choice differs (or no session is eligible), 2 an argument error or an input
// file that is missing, cannot be read or is not the shape expected, 70 an error the tool did not anticipate.

#include <odl/devkit/tool.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace odl::tools::measmod_g5_select {

/// A civil time of a CRD record (datetime.datetime without a zone): validated as the datetime constructor does (the year 1 to 9999, a month, a day of the month, hour 0-23, minute and second 0-59).
struct Time {
    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    friend bool operator==(const Time&, const Time&) = default;
    friend auto operator<=>(const Time&, const Time&) = default;
};

/// "%Y-%m-%d %H:%M:%S" of a time.
[[nodiscard]] std::string format_time(const Time& t);

/// One session of the file: what its H1-H4 records say and the number of its normal points (records of type 11; counted, not parsed).
struct Session {
    std::optional<std::int64_t> pad;   // the CDP pad number (H2's third field), none before an H2
    std::string name;                  // H2's second field
    std::optional<Time> start;         // H4
    std::optional<Time> end;
    std::optional<std::int64_t> alert; // H4's last field: the data-quality-alert indicator
    std::int64_t nps = 0;
};

/// An input that is missing, unreadable or not the shape this reads (the Python's OSError, IndexError, ValueError): exit 2.
struct BadInput : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The sessions of the normal-point file's text, in file order.
[[nodiscard]] std::vector<Session> sessions(const std::vector<std::string>& lines);

/// The pad the rule is about, the window, and the choice recorded in SPEC-measmod.md §8.9 (the start and the number of normal points).
inline constexpr std::int64_t kPad = 7090;
inline constexpr Time kWindowStart{2026, 1, 1, 0, 0, 0};
inline constexpr Time kWindowEnd{2026, 1, 3, 23, 0, 0};
inline constexpr char kChosenStart[] = "2026-01-02 04:04:45";
inline constexpr std::int64_t kChosenPoints = 18;
inline constexpr char kNpRelative[] = "data/cache/ilrs-lageos1-np-202601/lageos1_202601.np2";

struct Settings {
    std::filesystem::path root;
};
[[nodiscard]] std::filesystem::path default_root();
[[nodiscard]] Settings default_settings();

/// The tool.  `argv` is its arguments WITHOUT the program name.
[[nodiscard]] int run_on(const Settings& settings, const std::vector<std::string>& argv, odl::devkit::Streams io);
[[nodiscard]] int run(const std::vector<std::string>& argv, odl::devkit::Streams io);

}  // namespace odl::tools::measmod_g5_select
