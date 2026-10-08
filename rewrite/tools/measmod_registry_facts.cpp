// measmod_registry_facts.cpp — the registry's data facts, counted independently of the registry.
//
// Plan L0 step 8, group C8, ported to C++ (the user's directive of 2026-10-06) from tools/measmod_registry_facts.py (deleted by the same commit: `git show bc7cd6d:rewrite/tools/measmod_registry_facts.py`).
//
// SPEC-measmod.md MEAS-A-004 / MEAS-A-010 state the numbers of the pinned station files (SLRF2020 release 2026.02.05, the ILRS eccentricity file of 2026-05-27, the ITRF2020 SLR post-seismic event list).  The C++
// registry is tested against them, so they must come from somewhere that is NOT the registry: this reads the three cached files, by column and by whitespace field, and counts.  It reads nothing from `modules/`.
//
// A rule 3 record (plan §4): the header of the SLRF2020 file says "184 unique sites" and the file holds 186 distinct 4-character codes.  The difference is explained here by the file's own FILE/COMMENT history
// (Xian, 7329, and Ishioka, 7317, added in 2025 after the sentence was written), and checked: removing those two pads leaves exactly 184.
//
//   measmod_registry_facts [--root DIR] [--check]
//   exit 0 printed (and, with --check, every pinned number matched)   1 a pinned number did not match   2 an argument error, or an input file that is missing, cannot be read or is not the shape expected
//   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's output was never recorded.  The port printed, with --check, the output predicted BEFORE it existed by an INDEPENDENT computation in awk, written from the definitions
// (C8_registered/registry_control.awk, in this group's report files): the 22 counts, which are the 22 numbers of the Python's EXPECTED table and of SPEC-measmod MEAS-A-004, the post-seismic events of the 8
// sites, the three unplaced SODs that MEAS-A-004 names, and the verdict -- byte for byte.  (C8_proof_registration.txt, C-4, holds the registration and the result.)
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * The Python died with a traceback on a file that is not the shape it reads (a blank line in a SINEX block, a line with too few fields, a date that is not one or that no datetime holds, a header line without "for"); this REFUSES,
//     exit 2, naming the file and the fault.  A file that is missing, a directory, a file that is not UTF-8: REFUSED, exit 2, naming it.  Columns are slices by code point, as str slices are.
//   * int() reads an integer of any size; this reads the integers of 64 bits and refuses a longer one ("is beyond the range this reads").  Only a solution number could matter (the sums of a date overflow a
//     datetime long before): the Python would have sorted it.
//   * str.isdigit() of a post-seismic site number is read as "every character is a decimal digit" (Unicode category Nd): the superscripts and the circled digits, which isdigit() also admits, are not read as
//     digits.  The files have only ASCII, and no SINEX field holds another kind of digit.
//   * The Python sorted the solutions of one marker with list.sort() (timsort) over tuples (number, start, end) whose epochs are datetimes or None; here std::stable_sort orders them the same way (solution_less).  Both sorts
//     are stable and a stable sort by a strict weak order has ONE result, so on every input the Python sorted the list comes out the same.  Where the Python compared None with a datetime it raised TypeError, and so this
//     REFUSES ("two solutions of one marker have the same number and only one has an epoch"); which pairs are compared depends on the algorithm, so when three or more solutions of one number mix known and unknown
//     epochs the two may differ in whether they trip on it.  The real file has no such solutions.
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--root=DIR` is taken).  `-h` prints this tool's own text.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_registry_facts.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace dk = odl::devkit;

namespace odl::tools::measmod_registry_facts {

using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "measmod_registry_facts";

using Key = std::tuple<std::string, std::string, std::string>;   // pad, point, DOMES

// the lines between "+NAME" and "-NAME", without those that begin with "*"
std::vector<std::string> block(const std::vector<std::string>& lines, const std::string& name) {
    std::vector<std::string> out;
    bool on = false;
    for (const std::string& line : lines) {
        if (line.compare(0, name.size() + 1, "+" + name) == 0) {
            on = true;
            continue;
        }
        if (line.compare(0, name.size() + 1, "-" + name) == 0) break;
        if (on && line.compare(0, 1, "*") != 0) out.push_back(line);
    }
    return out;
}

// line[begin:end] in code points
std::string slice(const std::string& line, std::size_t begin, std::size_t end) {
    std::string out;
    std::size_t i = 0;
    std::size_t index = 0;
    while (i < line.size() && index < end) {
        std::size_t after = 0;
        (void)dk::code_point_at(line, i, after);
        if (index >= begin) out.append(line, i, after - i);
        i = after;
        ++index;
    }
    return out;
}

// the site key of a SITE/ID line: the pad (columns 2-5), the point (column 8) and the DOMES number (columns 10-18, stripped)
Key site_key(const std::string& line) { return Key{slice(line, 1, 5), slice(line, 7, 8), dk::strip_py(slice(line, 9, 18))}; }

// str.isdigit() of a field of a split line (which is never empty), read as: every character is a decimal digit
bool all_decimal(const std::string& field) {
    for (std::size_t i = 0; i < field.size();) {
        std::size_t after = 0;
        if (!dk::is_py_decimal(dk::code_point_at(field, i, after))) return false;
        i = after;
    }
    return true;
}

const std::string& last_field_of(const std::vector<std::string>& fields, const std::string& what) {
    if (fields.empty()) throw BadInput(what + ": a blank line where a record is wanted");
    return fields.back();
}

// int(token): an optional sign, decimal digits (of any script), single underscores BETWEEN digits
std::int64_t py_int(const std::string& token, const std::string& what) {
    const auto bad = [&] { return BadInput(what + ": '" + token + "' is not an integer"); };
    std::size_t i = 0;
    bool negative = false;
    if (i < token.size() && (token[i] == '+' || token[i] == '-')) {
        negative = token[i] == '-';
        ++i;
    }
    if (i >= token.size()) throw bad();
    std::int64_t value = 0;
    bool after_digit = false;
    bool after_underscore = false;
    while (i < token.size()) {
        std::size_t after = 0;
        const std::uint32_t cp = dk::code_point_at(token, i, after);
        if (cp == U'_') {
            if (!after_digit) throw bad();
            after_digit = false;
            after_underscore = true;
        } else {
            const int digit = dk::py_decimal_value(cp);
            if (digit < 0) throw bad();
            if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) throw BadInput(what + ": '" + token + "' is beyond the range this reads");
            value = value * 10 + digit;
            after_digit = true;
            after_underscore = false;
        }
        i = after;
    }
    if (after_underscore) throw bad();
    return negative ? -value : value;
}

}  // namespace

// days from 1970-01-01 to a date of the proleptic Gregorian calendar (Howard Hinnant's days_from_civil)
std::int64_t days_from_civil(std::int64_t year, int month, int day) {
    const std::int64_t y = year - (month <= 2 ? 1 : 0);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const auto yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153U * static_cast<unsigned>(month > 2 ? month - 3 : month + 9) + 2U) / 5U + static_cast<unsigned>(day) - 1U;
    const unsigned doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

// a SINEX epoch YY:DOY:SSSSS as seconds since 1970 (datetime(y, 1, 1) + timedelta(days=doy - 1, seconds=sec), the end of a day taken as the start of the next); none for 00:000:xxxxx, the open end or the unknown start.
// Where the Python's datetime or timedelta raised ValueError or OverflowError (a year or an instant beyond 0001-01-01 to 9999-12-31), this refuses; the sums are made without overflow whatever the tokens.
std::optional<std::int64_t> snx_epoch(const std::string& e, bool end) {
    std::vector<std::string> parts;
    std::size_t begin = 0;
    for (std::size_t colon = e.find(':'); colon != std::string::npos; colon = e.find(':', begin)) {
        parts.push_back(e.substr(begin, colon - begin));
        begin = colon + 1;
    }
    parts.push_back(e.substr(begin));
    if (parts.size() != 3) throw BadInput("an epoch '" + e + "' is not YY:DOY:SSSSS");
    const std::string& yy = parts[0];
    const std::string& doy = parts[1];
    const std::string& sec = parts[2];
    if (yy == "00" && doy == "000") return std::nullopt;   // open end (or unknown start)
    const std::int64_t year_two = py_int(yy, "the year of an epoch");
    const std::int64_t day = py_int(doy, "the day of an epoch");
    const std::int64_t seconds = py_int(sec, "the seconds of an epoch");
    // the year is 2000 + yy below 50 and 1900 + yy from 50 on; datetime(y, 1, 1) holds the years 1 to 9999: yy from -1999 (2000 - 1999 = 1) to 8099 (1900 + 8099 = 9999)
    if (year_two < -1999 || year_two > 8099) throw BadInput("the year of the epoch '" + e + "' is out of range");
    const std::int64_t y = year_two + (year_two < 50 ? 2000 : 1900);
    const auto out_of_range = [&] { return BadInput("the epoch '" + e + "' is out of range"); };
    // timedelta(days=days, seconds=into_day): the whole days of the seconds count as days (a negative number of seconds counts back from the start of the day)
    const bool end_of_day = end && seconds == 86399;   // the file's day-end convention (SPEC-measmod MEAS-R-002): the start of the next day
    std::int64_t days = end_of_day ? day : day - 1;
    std::int64_t into_day = end_of_day ? 0 : seconds;
    std::int64_t more_days = into_day / 86400;
    into_day %= 86400;
    if (into_day < 0) {
        into_day += 86400;
        --more_days;
    }
    if ((more_days > 0 && days > std::numeric_limits<std::int64_t>::max() - more_days) || (more_days < 0 && days < std::numeric_limits<std::int64_t>::min() - more_days)) throw out_of_range();
    days += more_days;
    // the instant must be a datetime's: from 0001-01-01 (day -719162 from 1970-01-01) to 9999-12-31 (day 2932896)
    const std::int64_t first_of_year = days_from_civil(y, 1, 1);
    if (days < -719162 - first_of_year || days > 2932896 - first_of_year) throw out_of_range();
    return (first_of_year + days) * 86400 + into_day;
}

namespace {

struct Solution {
    std::int64_t number;
    std::optional<std::int64_t> start;
    std::optional<std::int64_t> end;
};

// the order of the tuples (int, datetime-or-None, datetime-or-None) that list.sort() uses: where the Python would have compared None with a datetime it raised TypeError, and so this refuses
bool solution_less(const Solution& a, const Solution& b) {
    if (a.number != b.number) return a.number < b.number;
    const auto compare = [](const std::optional<std::int64_t>& x, const std::optional<std::int64_t>& y) -> int {
        if (x.has_value() != y.has_value()) throw BadInput("two solutions of one marker have the same number and only one has an epoch");
        if (!x) return 0;
        return *x < *y ? -1 : (*x > *y ? 1 : 0);
    };
    const int by_start = compare(a.start, b.start);
    if (by_start != 0) return by_start < 0;
    return compare(a.end, b.end) < 0;
}

}  // namespace

const std::vector<std::pair<std::string, std::int64_t>>& expected() {
    static const std::vector<std::pair<std::string, std::int64_t>> table = {
        {"slrf_site_id_rows", 483}, {"slrf_distinct_sods", 483}, {"slrf_pads", 186}, {"slrf_markers", 190}, {"slrf_markers_with_solution", 189},
        {"slrf_solutions", 237}, {"slrf_markers_with_more_than_one_soln", 28}, {"slrf_gaps_between_solutions", 48},
        {"slrf_header_sites", 184}, {"pads_added_after_header", 2},
        {"ecc_site_id_rows", 543}, {"ecc_distinct_sods", 542}, {"ecc_pads", 235}, {"ecc_identical_duplicate_rows", 1},
        {"pad_point_domes_disagreements", 0},
        {"placed_sods", 482}, {"unplaced_sods", 60}, {"unplaced_pad_absent", 57}, {"unplaced_sod_absent_on_known_pad", 2}, {"unplaced_marker_without_solution", 1},
        {"psd_sites", 8}, {"psd_events", 12},
    };
    return table;
}

Facts count(const std::vector<std::string>& slrf, const std::vector<std::string>& ecc, const std::vector<std::string>& psd) {
    std::vector<std::pair<std::string, std::int64_t>> got;
    const auto set_value = [&](const std::string& name, std::int64_t value) { got.emplace_back(name, value); };

    const std::vector<std::string> sid_s = block(slrf, "SITE/ID");
    const std::vector<std::string> sid_e = block(ecc, "SITE/ID");
    std::map<std::string, Key> sod_s;
    for (const std::string& line : sid_s) sod_s.insert_or_assign(last_field_of(dk::split_py(line), "SITE/ID of SLRF2020"), site_key(line));
    std::map<std::string, Key> sod_e;
    std::int64_t dup_identical = 0;
    for (const std::string& line : sid_e) {
        const std::string s = last_field_of(dk::split_py(line), "SITE/ID of the eccentricity file");
        const Key key = site_key(line);
        const auto found = sod_e.find(s);
        if (found != sod_e.end() && found->second == key) ++dup_identical;
        sod_e.insert_or_assign(s, key);
    }
    std::set<std::string> pads_s;
    std::set<std::string> pads_e;
    for (const auto& entry : sod_s) pads_s.insert(std::get<0>(entry.second));
    for (const auto& entry : sod_e) pads_e.insert(std::get<0>(entry.second));

    const std::vector<std::string> epochs = block(slrf, "SOLUTION/EPOCHS");
    std::map<std::pair<std::string, std::string>, std::vector<Solution>> solns;
    for (const std::string& line : epochs) {
        const std::vector<std::string> f = dk::split_py(line);
        if (f.size() < 6) throw BadInput("a SOLUTION/EPOCHS line has " + std::to_string(f.size()) + " fields, six are wanted");
        solns[{f[0], f[1]}].push_back(Solution{py_int(f[2], "a solution number"), snx_epoch(f[4], false), snx_epoch(f[5], true)});
    }
    std::int64_t gaps = 0;
    for (auto& entry : solns) {
        std::vector<Solution>& v = entry.second;
        std::stable_sort(v.begin(), v.end(), solution_less);
        for (std::size_t i = 0; i + 1 < v.size(); ++i) {
            if (v[i].end && v[i + 1].start && *v[i].end < *v[i + 1].start) ++gaps;
        }
    }
    std::set<std::pair<std::string, std::string>> markers_s;
    for (const auto& entry : sod_s) markers_s.insert({std::get<0>(entry.second), std::get<1>(entry.second)});

    set_value("slrf_site_id_rows", static_cast<std::int64_t>(sid_s.size()));
    set_value("slrf_distinct_sods", static_cast<std::int64_t>(sod_s.size()));
    set_value("slrf_pads", static_cast<std::int64_t>(pads_s.size()));
    set_value("slrf_markers", static_cast<std::int64_t>(markers_s.size()));
    set_value("slrf_markers_with_solution", static_cast<std::int64_t>(solns.size()));
    set_value("slrf_solutions", static_cast<std::int64_t>(epochs.size()));
    std::int64_t several = 0;
    for (const auto& entry : solns) {
        if (entry.second.size() > 1) ++several;
    }
    set_value("slrf_markers_with_more_than_one_soln", several);
    set_value("slrf_gaps_between_solutions", gaps);
    // the header's own count: "... for 184 unique sites." in the first 200 lines
    std::int64_t header_sites = -1;
    for (std::size_t i = 0; i < slrf.size() && i < 200; ++i) {
        const std::string& line = slrf[i];
        if (line.find("unique sites") == std::string::npos) continue;
        // line.split("for")[1].split()[0]: the first word of the text between the first "for" and the next
        const std::size_t first_for = line.find("for");
        if (first_for == std::string::npos) throw BadInput("the header line '" + line + "' has no 'for'");
        const std::size_t after_for = first_for + 3;
        const std::size_t next_for = line.find("for", after_for);
        const std::vector<std::string> words = dk::split_py(line.substr(after_for, next_for == std::string::npos ? std::string::npos : next_for - after_for));
        if (words.empty()) throw BadInput("the header line '" + line + "' has no number after 'for'");
        header_sites = py_int(words[0], "the header's count of unique sites");
        break;
    }
    set_value("slrf_header_sites", header_sites);
    std::int64_t added = 0;
    std::int64_t left = 0;
    for (const std::string& pad : pads_s) {
        if (pad == "7329" || pad == "7317") ++added;
        else ++left;
    }
    set_value("pads_added_after_header", left == header_sites ? added : -1);
    set_value("ecc_site_id_rows", static_cast<std::int64_t>(sid_e.size()));
    set_value("ecc_distinct_sods", static_cast<std::int64_t>(sod_e.size()));
    set_value("ecc_pads", static_cast<std::int64_t>(pads_e.size()));
    set_value("ecc_identical_duplicate_rows", dup_identical);

    std::int64_t disagree = 0;
    for (const auto& entry : sod_s) {
        const auto other = sod_e.find(entry.first);
        if (other != sod_e.end() && other->second != entry.second) ++disagree;
    }
    set_value("pad_point_domes_disagreements", disagree);
    std::int64_t placed = 0;
    std::int64_t unplaced = 0;
    std::int64_t pad_absent = 0;
    std::int64_t sod_absent = 0;
    std::int64_t without_solution = 0;
    std::vector<std::string> unplaced_on_known_pads;
    for (const auto& [sod, key] : sod_e) {
        const auto in_slrf = sod_s.find(sod);
        const bool placeable = in_slrf != sod_s.end() && in_slrf->second == key && solns.count({std::get<0>(in_slrf->second), std::get<1>(in_slrf->second)}) != 0;
        if (placeable) {
            ++placed;
            continue;
        }
        ++unplaced;
        const bool pad_known = pads_s.count(std::get<0>(key)) != 0;
        if (!pad_known) ++pad_absent;
        if (pad_known && in_slrf == sod_s.end()) ++sod_absent;
        if (in_slrf != sod_s.end() && solns.count({std::get<0>(in_slrf->second), std::get<1>(in_slrf->second)}) == 0) ++without_solution;
        if (pad_known) unplaced_on_known_pads.push_back(sod);
    }
    set_value("placed_sods", placed);
    set_value("unplaced_sods", unplaced);
    set_value("unplaced_pad_absent", pad_absent);
    set_value("unplaced_sod_absent_on_known_pad", sod_absent);
    set_value("unplaced_marker_without_solution", without_solution);

    // the post-seismic events: a record line begins with the site number (digits) and has the epoch, with two colons, as its fourth field
    std::vector<std::pair<std::string, std::vector<std::string>>> events;
    std::int64_t event_count = 0;
    for (const std::string& line : psd) {
        const std::vector<std::string> f = dk::split_py(line);
        if (f.size() < 4) continue;
        if (!all_decimal(f[0]) || std::count(f[3].begin(), f[3].end(), ':') != 2) continue;
        auto site = std::find_if(events.begin(), events.end(), [&](const auto& e) { return e.first == f[0]; });
        if (site == events.end()) {
            events.emplace_back(f[0], std::vector<std::string>{});
            site = events.end() - 1;
        }
        site->second.push_back(f[3]);
        ++event_count;
    }
    set_value("psd_sites", static_cast<std::int64_t>(events.size()));
    set_value("psd_events", event_count);

    std::sort(unplaced_on_known_pads.begin(), unplaced_on_known_pads.end());
    return Facts{std::move(got), std::move(events), std::move(unplaced_on_known_pads)};
}

std::filesystem::path default_root() {
#ifdef ODL_TREE_ROOT
    return std::filesystem::path(ODL_TREE_ROOT);
#else
    return std::filesystem::current_path();
#endif
}

Settings default_settings() { return Settings{default_root()}; }

namespace {

const char kUsageText[] = "usage: measmod_registry_facts [-h] [--root ROOT] [--check]\n";

const char kHelpText[] =
    "\n"
    "The registry's data facts, counted independently of the registry: SPEC-measmod.md MEAS-A-004 / MEAS-A-010 state the numbers of the pinned station files (SLRF2020 release 2026.02.05, the ILRS\n"
    "eccentricity file of 2026-05-27, the ITRF2020 SLR post-seismic event list); this reads the three cached files by column and by whitespace field, and counts.  It reads nothing from modules/.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --root ROOT  the tree whose data/cache holds the files (default: the tree this tool was built from)\n"
    "  --check      exit 1 unless every pinned number matches\n"
    "\n"
    "exit codes: 0 printed (and, with --check, every pinned number matched)   1 a pinned number did not match\n"
    "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";

std::string repr_list(const std::vector<std::string>& items) {
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) out += (i > 0 ? ", " : "") + dk::py_repr(items[i]);
    return out + "]";
}

}  // namespace

int run_on(const Settings& settings, const std::vector<std::string>& argv, Streams io) {
    try {
        bool check = false;
        std::filesystem::path root = settings.root;
        const auto usage_error = [&](const std::string& what) {
            io.err << kUsageText << kTool << ": error: " << what << '\n';
            return kArgument;
        };
        for (std::size_t i = 0; i < argv.size(); ++i) {
            std::string opt = argv[i];
            std::string value;
            bool has_value = false;
            const std::size_t eq = opt.find('=');
            if (opt.rfind("--", 0) == 0 && eq != std::string::npos) {
                value = opt.substr(eq + 1);
                opt = opt.substr(0, eq);
                has_value = true;
            }
            if (opt == "-h" || opt == "--help") {
                io.out << kUsageText << kHelpText;
                return kOk;
            }
            if (opt == "--check" && !has_value) {
                check = true;
            } else if (opt == "--root") {
                if (!has_value) {
                    if (i + 1 >= argv.size() || argv[i + 1].rfind("--", 0) == 0) return usage_error("argument --root: expected one argument");
                    value = argv[++i];
                }
                root = value;
            } else {
                return usage_error("unrecognized arguments: " + argv[i]);
            }
        }

        Facts facts;
        try {
            const std::vector<std::string> slrf = dk::splitlines_py(dk::read_text(root / kSlrfRelative));
            const std::vector<std::string> ecc = dk::splitlines_py(dk::read_text(root / kEccRelative));
            const std::vector<std::string> psd = dk::splitlines_py(dk::read_text(root / kPsdRelative));
            facts = count(slrf, ecc, psd);
        } catch (const std::runtime_error& exc) {   // (BadInput is one)
            io.err << kTool << ": " << exc.what() << " (run `tools/bootstrap.sh`)\n";
            return kArgument;
        }

        std::size_t width = 0;
        for (const auto& entry : facts.counts) width = std::max(width, dk::code_points(entry.first));
        std::int64_t bad = 0;
        for (const auto& [name, value] : facts.counts) {
            std::string mark;
            if (check) {
                const auto& table = expected();
                const auto wanted = std::find_if(table.begin(), table.end(), [&](const auto& e) { return e.first == name; });
                if (wanted == table.end() || wanted->second != value) {
                    mark = "   <- EXPECTED " + (wanted == table.end() ? std::string("?") : std::to_string(wanted->second));
                    ++bad;
                }
            }
            io.out << "  " << dk::pad_right(name, width) << "  " << dk::pad_left(std::to_string(value), 5) << mark << '\n';
        }
        io.out << "  PSD events (site: SINEX epochs): {";
        for (std::size_t i = 0; i < facts.events.size(); ++i) io.out << (i > 0 ? ", " : "") << dk::py_repr(facts.events[i].first) << ": " << repr_list(facts.events[i].second);
        io.out << "}\n";
        io.out << "  unplaced SODs on pads SLRF2020 lists: " << repr_list(facts.unplaced_on_known_pads) << '\n';
        if (check) io.out << (bad == 0 ? std::string("ok       every pinned number matches") : "FAILED   " + std::to_string(bad) + " number(s) differ") << '\n';
        return bad != 0 ? kFailed : kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::measmod_registry_facts

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::measmod_registry_facts::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
