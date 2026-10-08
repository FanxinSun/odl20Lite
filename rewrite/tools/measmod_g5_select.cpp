// measmod_g5_select.cpp — the G5 pass-selection rule of SPEC-measmod.md §8.9, applied to the normal-point file's METADATA.
//
// Plan L0 step 8, group C8, ported to C++ (the user's directive of 2026-10-06) from tools/measmod_g5_select.py (deleted by the same commit: `git show bc7cd6d:rewrite/tools/measmod_g5_select.py`).
//
// THE RULE (written and committed, 0a48ec0, before this tool existed): among the CRD sessions of the pinned normal-point file whose station is Yarragadee (CDP pad 7090, the second field of the `H2` record)
// and which lie inside the window -- start >= 2026-01-01 00:00:00 UTC and end <= 2026-01-03 23:00:00 UTC -- and whose `H4` data-quality-alert indicator is 0, take the session with the most normal points
// (records of type 11), ties broken by the earliest start; a session the observation builder refuses for a reason of its own metadata is skipped and recorded (the builder is C++: the envelope's test applies
// that clause and says so; this tool applies the rest).
//
// No observed range is read: the normal points are COUNTED, their fields are not parsed.  The tool prints every Yarragadee session of the file with the rule's verdict on it.
//
//   measmod_g5_select [--check] [--root DIR]
//     (--check: exit 1 unless the chosen session is the one recorded in SPEC-measmod.md §8.9 / kChosen* in the header)
//   exit 0 printed (and, with --check, the choice reproduced)   1 the choice differs, or no session is eligible   2 an argument error, or an input file that is missing, cannot be read or is not the shape expected
//   70 an error the tool did not anticipate
//
// THE PROOF OF THE PORT.  The tool's plain output was kept by the L6 report (round9/g5_selection.out, 1,472 bytes, 28 lines, sha256 dbb8ad9c...), the evidence that SPEC-measmod.md §8.9 and PROVENANCE name.  The
// port printed it BYTE FOR BYTE, against an EMPTY substitution list (the output names no tool) registered before the comparison; with --check it prints the same followed by the one line "ok       the recorded
// choice is reproduced".  (C8_proof_registration.txt, C-3, in this group's report files, holds the registration and the result.)  What no record holds -- the failures, the refusals -- stands on tests.
//
// WHERE THE PORT DIFFERS, deliberately and said again at the place:
//   * `--root DIR` is new (the Python took the root from the script's own place); the default is the tree this tool was built from.
//   * The Python died with a traceback on a file that is not the shape it reads (an H2 or an H4 with too few fields, a field that is not an integer, a date that is not one, a session of the pad with no H4);
//     this REFUSES, naming the line, exit 2.  A file that is missing, a directory, a file that is not UTF-8: REFUSED, exit 2, naming it.
//   * int() is Python's: a sign, decimal digits of any script, single underscores between digits.  The record tag is the first two characters of the line, "H1" in either case (str.lower() maps no other
//     character to an ASCII letter h).
//   * The Python's argparse took any unambiguous abbreviation of an option; here the names are exact (`--root=DIR` is taken).  `-h` prints this tool's own text.

#include <odl/devkit/fs.hpp>
#include <odl/devkit/pytext.hpp>
#include <odl/devkit/text.hpp>
#include <odl/devkit/tool.hpp>

#include "measmod_g5_select.hpp"

#include <algorithm>
#include <iostream>
#include <limits>

namespace dk = odl::devkit;

namespace odl::tools::measmod_g5_select {

using dk::Streams;

namespace {

constexpr int kOk = 0;
constexpr int kFailed = 1;
constexpr int kArgument = 2;
constexpr int kInternal = 70;
constexpr const char kTool[] = "measmod_g5_select";

// int(token): an optional sign, decimal digits (of any script), single underscores BETWEEN digits; nothing else
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
            if (!after_digit) throw bad();   // a leading underscore, or two in a row
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
    if (after_underscore) throw bad();   // a trailing underscore
    return negative ? -value : value;
}

bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int days_in_month(int y, int m) {
    constexpr int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && leap(y) ? 29 : kDays[m - 1];
}

// datetime.datetime(y, mo, d, h, mi, s): the constructor's checks
Time make_time(std::int64_t y, std::int64_t mo, std::int64_t d, std::int64_t h, std::int64_t mi, std::int64_t s, const std::string& where) {
    if (y < 1 || y > 9999) throw BadInput(where + ": year " + std::to_string(y) + " is out of range");
    if (mo < 1 || mo > 12) throw BadInput(where + ": month " + std::to_string(mo) + " is out of range");
    if (d < 1 || d > days_in_month(static_cast<int>(y), static_cast<int>(mo))) throw BadInput(where + ": day " + std::to_string(d) + " is out of range for the month");
    if (h < 0 || h > 23) throw BadInput(where + ": hour " + std::to_string(h) + " is out of range");
    if (mi < 0 || mi > 59) throw BadInput(where + ": minute " + std::to_string(mi) + " is out of range");
    if (s < 0 || s > 59) throw BadInput(where + ": second " + std::to_string(s) + " is out of range");
    return Time{static_cast<int>(y), static_cast<int>(mo), static_cast<int>(d), static_cast<int>(h), static_cast<int>(mi), static_cast<int>(s)};
}

std::string two(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

}  // namespace

std::string format_time(const Time& t) {
    std::string year = std::to_string(t.year);
    year.insert(0, year.size() < 4 ? 4 - year.size() : 0, '0');
    return year + "-" + two(t.month) + "-" + two(t.day) + " " + two(t.hour) + ":" + two(t.minute) + ":" + two(t.second);
}

std::vector<Session> sessions(const std::vector<std::string>& lines) {
    std::vector<Session> out;
    bool have_current = false;
    std::size_t line_number = 0;
    for (const std::string& line : lines) {
        ++line_number;
        const std::string where = "line " + std::to_string(line_number);
        const bool is_h = line.size() >= 2 && (line[0] == 'H' || line[0] == 'h');
        const char second = line.size() >= 2 ? line[1] : '\0';
        if (is_h && second == '1') {
            out.emplace_back();
            have_current = true;
        } else if (!have_current) {
            continue;
        } else if (is_h && second == '2') {
            const std::vector<std::string> f = dk::split_py(line);
            if (f.size() < 3) throw BadInput(where + ": an H2 record has " + std::to_string(f.size()) + " fields, three are wanted");
            out.back().name = f[1];
            out.back().pad = py_int(f[2], where + ": the pad of an H2 record");
        } else if (is_h && second == '4') {
            const std::vector<std::string> f = dk::split_py(line);
            if (f.size() < 14) throw BadInput(where + ": an H4 record has " + std::to_string(f.size()) + " fields, fourteen are wanted");
            std::int64_t v[12];
            for (std::size_t k = 0; k < 12; ++k) v[k] = py_int(f[2 + k], where + ": a time field of an H4 record");
            out.back().start = make_time(v[0], v[1], v[2], v[3], v[4], v[5], where + ": the start of an H4 record");
            out.back().end = make_time(v[6], v[7], v[8], v[9], v[10], v[11], where + ": the end of an H4 record");
            out.back().alert = py_int(f.back(), where + ": the last field of an H4 record");
        } else if (line.compare(0, 2, "11") == 0) {
            ++out.back().nps;   // counted, not parsed: no range enters
        }
    }
    return out;
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

const char kUsageText[] = "usage: measmod_g5_select [-h] [--check] [--root ROOT]\n";

const char kHelpText[] =
    "\n"
    "The G5 pass-selection rule of SPEC-measmod.md section 8.9, applied to the normal-point file's metadata: among the CRD sessions of Yarragadee (pad 7090) inside the window with the data-quality-alert\n"
    "indicator 0, the one with the most normal points, ties broken by the earliest start.  The tool prints every Yarragadee session of the file with the rule's verdict.\n"
    "\n"
    "options:\n"
    "  -h, --help   show this help and exit\n"
    "  --check      exit 1 unless the chosen session is the one recorded in SPEC-measmod.md section 8.9\n"
    "  --root ROOT  the tree whose data/cache holds the file (default: the tree this tool was built from)\n"
    "\n"
    "exit codes: 0 printed (and, with --check, the choice reproduced)   1 the choice differs, or no session is eligible\n"
    "            2 an argument error, or an input file that is missing, cannot be read or is not the shape expected\n";

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

        std::vector<Session> all;
        try {
            all = sessions(dk::splitlines_py(dk::read_text(root / kNpRelative)));
        } catch (const std::runtime_error& exc) {   // (BadInput is one)
            io.err << kTool << ": " << exc.what() << " (run `tools/bootstrap.sh`)\n";
            return kArgument;
        }
        std::vector<const Session*> mine;
        for (const Session& s : all) {
            if (s.pad && *s.pad == kPad) mine.push_back(&s);
        }
        struct Row {
            const Session* session;
            bool inside;
            bool eligible;
        };
        std::vector<Row> rows;
        for (const Session* s : mine) {
            if (!s->start) {   // (the start, the end and the alert are set together, by the one H4 record: a session has all three or none)
                io.err << kTool << ": a session of pad " << kPad << " has no H4 record (run `tools/bootstrap.sh`)\n";
                return kArgument;
            }
            const bool inside = kWindowStart <= *s->start && *s->end <= kWindowEnd;
            rows.push_back(Row{s, inside, inside && *s->alert == 0});
        }
        io.out << all.size() << " sessions in the file, " << mine.size() << " of pad " << kPad << '\n';
        io.out << std::count_if(rows.begin(), rows.end(), [](const Row& r) { return !r.inside; }) << " of them lie outside the window (not listed)\n";
        io.out << dk::pad_left("start (UTC)", 19) << ' ' << dk::pad_left("end (UTC)", 19) << ' ' << dk::pad_left("alert", 5) << ' ' << dk::pad_left("NPs", 4) << "  verdict\n";
        std::vector<Row> by_start = rows;
        std::stable_sort(by_start.begin(), by_start.end(), [](const Row& a, const Row& b) { return *a.session->start < *b.session->start; });
        for (const Row& row : by_start) {
            if (!row.inside) continue;
            const Session& s = *row.session;
            const std::string verdict = row.eligible ? "eligible" : "quality alert " + std::to_string(*s.alert);
            io.out << format_time(*s.start) << ' ' << format_time(*s.end) << ' ' << dk::pad_left(std::to_string(*s.alert), 5) << ' ' << dk::pad_left(std::to_string(s.nps), 4) << "  " << verdict << '\n';
        }
        std::vector<const Session*> ranked;
        for (const Row& row : rows) {
            if (row.eligible) ranked.push_back(row.session);
        }
        std::stable_sort(ranked.begin(), ranked.end(), [](const Session* a, const Session* b) { return a->nps != b->nps ? a->nps > b->nps : *a->start < *b->start; });
        io.out << "\nthe rule's order (most normal points, ties by the earliest start):\n";
        for (std::size_t i = 0; i < ranked.size() && i < 6; ++i) io.out << "  " << i + 1 << ". " << format_time(*ranked[i]->start) << "  " << ranked[i]->nps << " normal points\n";
        if (ranked.empty()) {
            io.err << "no eligible session\n";
            return kFailed;
        }
        const Session& top = *ranked.front();
        io.out << "\nCHOSEN: the session of " << format_time(*top.start) << " UTC (" << top.name << ", pad " << *top.pad << "), " << top.nps << " normal points\n";
        if (ranked.size() > 1 && ranked[1]->nps == top.nps) {
            io.out << "  (a tie of " << top.nps << " points with " << format_time(*ranked[1]->start) << ", broken by the earlier start)\n";
        }
        if (check) {
            const bool ok = format_time(*top.start) == kChosenStart && top.nps == kChosenPoints;
            if (ok) {
                io.out << "ok       the recorded choice is reproduced\n";
            } else {
                io.out << "FAILED   the rule now chooses " << format_time(*top.start) << " with " << top.nps << " points, not ('" << kChosenStart << "', " << kChosenPoints << ")\n";
            }
            return ok ? kOk : kFailed;
        }
        return kOk;
    } catch (const std::exception& exc) {
        io.err << kTool << ": internal error: " << exc.what() << '\n';
        return kInternal;
    }
}

int run(const std::vector<std::string>& argv, Streams io) { return run_on(default_settings(), argv, io); }

}  // namespace odl::tools::measmod_g5_select

#ifndef ODL_TOOL_NO_MAIN
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    return odl::tools::measmod_g5_select::run(args, odl::devkit::Streams{std::cout, std::cerr});
}
#endif
