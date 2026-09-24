// antex.cpp — SPEC-io-formats.md §3.7 (`ANTEX14`). Label in columns 61-80.

#include <odl/io/antex.hpp>

#include <cctype>
#include <charconv>
#include <cmath>
#include <sstream>

namespace odl::io {
namespace {

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t nl = text.find('\n', start);
        std::string_view line = (nl == std::string_view::npos) ? text.substr(start)
                                                                : text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.push_back(line);
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    if (!lines.empty() && lines.back().empty()) lines.pop_back();
    return lines;
}

std::string trim(std::string_view s) {
    std::size_t b = s.find_first_not_of(' ');
    if (b == std::string_view::npos) return "";
    std::size_t e = s.find_last_not_of(' ');
    return std::string(s.substr(b, e - b + 1));
}

std::vector<std::string> tokenize(std::string_view line) {
    std::vector<std::string> toks;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        std::size_t start = i;
        while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) ++i;
        if (i > start) toks.push_back(std::string(line.substr(start, i - start)));
    }
    return toks;
}

std::string label_of(std::string_view line) {
    if (line.size() <= 60) return "";
    return trim(line.substr(60));
}

std::string_view data_of(std::string_view line) {
    return line.size() > 60 ? line.substr(0, 60) : line;
}

std::string col(std::string_view s, std::size_t first, std::size_t last) {
    if (s.size() < first) return "";
    std::size_t end = std::min(last, s.size());
    return std::string(s.substr(first - 1, end - first + 1));
}

odl::Result<double, AntexError> to_double(const std::string& raw, std::string_view field) {
    std::string t = trim(raw);
    if (t.empty()) {
        return odl::err(AntexError{"IOFM-F-001", "ANTEX: field '" + std::string(field) + "' is blank"});
    }
    try {
        std::size_t consumed = 0;
        double v = std::stod(t, &consumed);
        if (consumed != t.size()) throw std::invalid_argument("trailing");
        return v;
    } catch (const std::exception&) {
        return odl::err(AntexError{"IOFM-F-001", "ANTEX: field '" + std::string(field) +
            "' is not a number: '" + t + "'"});
    }
}

odl::Result<int, AntexError> to_int(const std::string& raw, std::string_view field) {
    std::string t = trim(raw);
    int v = 0;
    auto [ptr, ec] = std::from_chars(t.data(), t.data() + t.size(), v);
    if (ec != std::errc{} || ptr != t.data() + t.size()) {
        return odl::err(AntexError{"IOFM-F-001", "ANTEX: field '" + std::string(field) +
            "' is not an integer: '" + t + "'"});
    }
    return v;
}

}  // namespace

odl::Result<AntexFile, AntexError> read_antex(std::string_view text) {
    AntexFile file;
    auto lines = split_lines(text);
    std::size_t i = 0;

    bool have_version = false, have_pcv = false;
    for (; i < lines.size(); ++i) {
        std::string lab = label_of(lines[i]);
        auto toks = tokenize(data_of(lines[i]));
        if (lab == "ANTEX VERSION / SYST") {
            if (toks.size() < 2) return odl::err(AntexError{"IOFM-F-001", "ANTEX VERSION / SYST needs 2 fields"});
            auto v = to_double(toks[0], "version"); if (!v.has_value()) return odl::err(v.error());
            file.header.version = *v;
            file.header.satellite_system = toks[1].empty() ? 'M' : toks[1][0];
            have_version = true;
        } else if (lab == "PCV TYPE / REFANT") {
            std::string d(data_of(lines[i]));
            char pcv = col(d, 1, 1).empty() ? 'A' : col(d, 1, 1)[0];
            if (pcv != 'A' && pcv != 'R') {
                return odl::err(AntexError{"IOFM-F-008", "ANTEX PCV TYPE is '" + std::string(1, pcv) +
                    "', not 'A' or 'R'"});
            }
            file.header.pcv_type = (pcv == 'A') ? AntexPcvType::Absolute : AntexPcvType::Relative;
            file.header.reference_antenna = trim(col(d, 21, 40));
            have_pcv = true;
        } else if (lab == "END OF HEADER") {
            ++i;
            break;
        }
        // COMMENT and any other header label: skipped, not modelled (§3.7's own scope).
    }
    if (!have_version) return odl::err(AntexError{"IOFM-F-001", "ANTEX file has no ANTEX VERSION / SYST record"});
    if (!have_pcv) return odl::err(AntexError{"IOFM-F-001", "ANTEX file has no PCV TYPE / REFANT record"});

    for (; i < lines.size(); ++i) {
        std::string lab = label_of(lines[i]);
        if (lab != "START OF ANTENNA") continue;
        AntexAntenna ant;
        ++i;
        for (; i < lines.size(); ++i) {
            std::string alab = label_of(lines[i]);
            std::string d(data_of(lines[i]));
            if (alab == "END OF ANTENNA") { ++i; break; }
            if (alab == "TYPE / SERIAL NO") {
                ant.antenna_type = trim(col(d, 1, 20));
                ant.serial_or_sat_code = trim(col(d, 21, 40));
            } else if (alab == "DAZI") {
                auto v = to_double(col(d, 3, 8), "DAZI"); if (!v.has_value()) return odl::err(v.error());
                ant.dazi_deg = *v;
            } else if (alab == "ZEN1 / ZEN2 / DZEN") {
                auto z1 = to_double(col(d, 3, 8), "ZEN1"); if (!z1.has_value()) return odl::err(z1.error());
                auto z2 = to_double(col(d, 9, 14), "ZEN2"); if (!z2.has_value()) return odl::err(z2.error());
                auto dz = to_double(col(d, 15, 20), "DZEN"); if (!dz.has_value()) return odl::err(dz.error());
                ant.zen1_deg = *z1; ant.zen2_deg = *z2; ant.dzen_deg = *dz;
            } else if (alab == "# OF FREQUENCIES") {
                auto v = to_int(col(d, 1, 6), "# OF FREQUENCIES"); if (!v.has_value()) return odl::err(v.error());
                ant.num_frequencies = *v;
            } else if (alab == "START OF FREQUENCY") {
                AntexFrequency freq;
                freq.code = trim(col(d, 4, 6));
                ++i;
                // NORTH / EAST / UP
                if (i >= lines.size() || label_of(lines[i]) != "NORTH / EAST / UP") {
                    return odl::err(AntexError{"IOFM-F-001",
                        "ANTEX frequency section is missing its own NORTH / EAST / UP record"});
                }
                std::string neu(data_of(lines[i]));
                auto n = to_double(col(neu, 1, 10), "north"); if (!n.has_value()) return odl::err(n.error());
                auto e = to_double(col(neu, 11, 20), "east"); if (!e.has_value()) return odl::err(e.error());
                auto u = to_double(col(neu, 21, 30), "up"); if (!u.has_value()) return odl::err(u.error());
                freq.north_mm = *n; freq.east_mm = *e; freq.up_mm = *u;
                ++i;
                // NOAZI row.
                if (i >= lines.size()) return odl::err(AntexError{"IOFM-F-001", "ANTEX frequency section ends before its own NOAZI row"});
                auto noazi_toks = tokenize(lines[i]);
                if (noazi_toks.empty() || noazi_toks[0] != "NOAZI") {
                    return odl::err(AntexError{"IOFM-F-001", "ANTEX frequency section's first pattern row is not NOAZI"});
                }
                for (std::size_t k = 1; k < noazi_toks.size(); ++k) {
                    auto v = to_double(noazi_toks[k], "NOAZI value"); if (!v.has_value()) return odl::err(v.error());
                    freq.noazi_mm.push_back(*v);
                }
                ++i;
                // Azimuth-dependent rows, if DAZI > 0, until END OF FREQUENCY.
                while (i < lines.size() && label_of(lines[i]) != "END OF FREQUENCY" &&
                       label_of(lines[i]) != "START OF FREQ RMS") {
                    auto az_toks = tokenize(lines[i]);
                    if (!az_toks.empty()) {
                        auto az = to_double(az_toks[0], "azimuth"); if (!az.has_value()) return odl::err(az.error());
                        std::vector<double> vals;
                        for (std::size_t k = 1; k < az_toks.size(); ++k) {
                            auto v = to_double(az_toks[k], "azimuth pattern value");
                            if (!v.has_value()) return odl::err(v.error());
                            vals.push_back(*v);
                        }
                        freq.azimuth_grid_mm.emplace_back(*az, std::move(vals));
                    }
                    ++i;
                }
                // Skip an RMS section entirely, if present (§3.7's own scope: not modelled).
                if (i < lines.size() && label_of(lines[i]) == "START OF FREQ RMS") {
                    while (i < lines.size() && label_of(lines[i]) != "END OF FREQ RMS") ++i;
                }
                if (i >= lines.size() || label_of(lines[i]) != "END OF FREQUENCY") {
                    return odl::err(AntexError{"IOFM-F-001", "ANTEX frequency section has no END OF FREQUENCY"});
                }
                ant.frequencies.push_back(std::move(freq));
                // outer loop's own ++i (from the for-statement) advances past END OF FREQUENCY
            }
            // VALID FROM / VALID UNTIL / SINEX CODE / METH.../COMMENT: skipped (§3.7's own scope).
        }
        file.antennas.push_back(std::move(ant));
        --i;  // outer for-loop's own ++i will advance past the line START OF ANTENNA's search left us on
    }

    return file;
}

namespace {
std::string rj(double v, int w, int decimals) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(decimals);
    os << v;
    std::string s = os.str();
    if (static_cast<int>(s.size()) < w) s = std::string(static_cast<std::size_t>(w) - s.size(), ' ') + s;
    return s;
}
std::string lj(const std::string& s, std::size_t w) {
    if (s.size() >= w) return s.substr(0, w);
    return s + std::string(w - s.size(), ' ');
}
std::string labelled(const std::string& data, const std::string& label) {
    return lj(data, 60) + lj(label, 20);
}
}  // namespace

odl::Result<std::string, AntexError> write_antex(const AntexFile& file) {
    std::ostringstream out;
    {
        std::ostringstream d;
        d.setf(std::ios::fixed); d.precision(1);
        d << file.header.version << std::string(12, ' ') << file.header.satellite_system;
        out << labelled(d.str(), "ANTEX VERSION / SYST") << "\n";
    }
    {
        std::string d(1, file.header.pcv_type == AntexPcvType::Absolute ? 'A' : 'R');
        d += std::string(19, ' ') + lj(file.header.reference_antenna, 20);
        out << labelled(d, "PCV TYPE / REFANT") << "\n";
    }
    out << labelled("", "END OF HEADER") << "\n";

    for (const auto& ant : file.antennas) {
        out << labelled("", "START OF ANTENNA") << "\n";
        out << labelled(lj(ant.antenna_type, 20) + lj(ant.serial_or_sat_code, 20), "TYPE / SERIAL NO") << "\n";
        out << labelled("  " + rj(ant.dazi_deg, 6, 1), "DAZI") << "\n";
        out << labelled("  " + rj(ant.zen1_deg, 6, 1) + rj(ant.zen2_deg, 6, 1) + rj(ant.dzen_deg, 6, 1),
                        "ZEN1 / ZEN2 / DZEN") << "\n";
        out << labelled(rj(static_cast<double>(ant.num_frequencies), 6, 0), "# OF FREQUENCIES") << "\n";
        for (const auto& f : ant.frequencies) {
            out << labelled("   " + lj(f.code, 3), "START OF FREQUENCY") << "\n";
            out << labelled(rj(f.north_mm, 10, 2) + rj(f.east_mm, 10, 2) + rj(f.up_mm, 10, 2),
                            "NORTH / EAST / UP") << "\n";
            out << "   NOAZI";
            for (double v : f.noazi_mm) out << rj(v, 8, 2);
            out << "\n";
            for (const auto& [az, vals] : f.azimuth_grid_mm) {
                out << rj(az, 8, 1);
                for (double v : vals) out << rj(v, 8, 2);
                out << "\n";
            }
            out << labelled("   " + lj(f.code, 3), "END OF FREQUENCY") << "\n";
        }
        out << labelled("", "END OF ANTENNA") << "\n";
    }
    return out.str();
}

bool operator==(const AntexHeader& a, const AntexHeader& b) noexcept {
    return std::abs(a.version - b.version) < 1e-6 && a.satellite_system == b.satellite_system &&
           a.pcv_type == b.pcv_type && a.reference_antenna == b.reference_antenna;
}
bool operator==(const AntexFrequency& a, const AntexFrequency& b) noexcept {
    if (a.code != b.code || std::abs(a.north_mm - b.north_mm) > 1e-6 ||
        std::abs(a.east_mm - b.east_mm) > 1e-6 || std::abs(a.up_mm - b.up_mm) > 1e-6 ||
        a.noazi_mm.size() != b.noazi_mm.size() || a.azimuth_grid_mm.size() != b.azimuth_grid_mm.size())
        return false;
    for (std::size_t k = 0; k < a.noazi_mm.size(); ++k)
        if (std::abs(a.noazi_mm[k] - b.noazi_mm[k]) > 1e-6) return false;
    for (std::size_t k = 0; k < a.azimuth_grid_mm.size(); ++k) {
        if (std::abs(a.azimuth_grid_mm[k].first - b.azimuth_grid_mm[k].first) > 1e-6) return false;
        if (a.azimuth_grid_mm[k].second.size() != b.azimuth_grid_mm[k].second.size()) return false;
        for (std::size_t j = 0; j < a.azimuth_grid_mm[k].second.size(); ++j)
            if (std::abs(a.azimuth_grid_mm[k].second[j] - b.azimuth_grid_mm[k].second[j]) > 1e-6) return false;
    }
    return true;
}
bool operator==(const AntexAntenna& a, const AntexAntenna& b) noexcept {
    return a.antenna_type == b.antenna_type && a.serial_or_sat_code == b.serial_or_sat_code &&
           std::abs(a.dazi_deg - b.dazi_deg) < 1e-6 && std::abs(a.zen1_deg - b.zen1_deg) < 1e-6 &&
           std::abs(a.zen2_deg - b.zen2_deg) < 1e-6 && std::abs(a.dzen_deg - b.dzen_deg) < 1e-6 &&
           a.num_frequencies == b.num_frequencies && a.frequencies == b.frequencies;
}
bool operator==(const AntexFile& a, const AntexFile& b) noexcept {
    return a.header == b.header && a.antennas == b.antennas;
}

}  // namespace odl::io
