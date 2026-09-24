// sinex.cpp — SPEC-io-formats.md §3.6 (`SINEX2`), general block structure only.

#include <odl/io/sinex.hpp>

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

}  // namespace

odl::Result<SinexFile, SinexError> read_sinex(std::string_view text) {
    auto lines = split_lines(text);
    if (lines.empty() || lines[0].substr(0, 5) != "%=SNX") {
        return odl::err(SinexError{"IOFM-F-006", "SINEX file does not begin with a '%=SNX' header line"});
    }

    SinexFile file;
    {
        auto toks = tokenize(lines[0]);
        if (toks.size() < 10) {
            return odl::err(SinexError{"IOFM-F-001",
                "SINEX header line has fewer than the ten mandatory fields"});
        }
        SinexHeader h;
        try {
            std::size_t consumed = 0;
            h.format_version = std::stod(toks[1], &consumed);
            if (consumed != toks[1].size()) throw std::invalid_argument("trailing");
        } catch (const std::exception&) {
            return odl::err(SinexError{"IOFM-F-001", "SINEX header: format version '" + toks[1] +
                "' is not a number"});
        }
        h.file_agency_code = toks[2];
        h.creation_time = toks[3];
        h.data_agency_code = toks[4];
        h.data_start_time = toks[5];
        h.data_end_time = toks[6];
        h.observation_code = toks[7].empty() ? ' ' : toks[7][0];
        int ne = 0;
        auto [ptr, ec] = std::from_chars(toks[8].data(), toks[8].data() + toks[8].size(), ne);
        if (ec != std::errc{} || ptr != toks[8].data() + toks[8].size()) {
            return odl::err(SinexError{"IOFM-F-001", "SINEX header: number of estimates '" + toks[8] +
                "' is not an integer"});
        }
        h.number_of_estimates = ne;
        h.constraint_code = toks[9].empty() ? ' ' : toks[9][0];
        for (std::size_t k = 10; k < toks.size(); ++k) h.solution_contents += toks[k];
        file.header = h;
    }

    SinexBlock* current = nullptr;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        std::string_view line = lines[i];
        int ln = static_cast<int>(i) + 1;
        if (line.empty()) continue;
        char c0 = line[0];
        if (c0 == '%') {
            if (line.substr(0, 7) == "%ENDSNX") break;
            continue;  // an unrecognised %-line elsewhere is tolerated, not this reader's own concern
        }
        if (c0 == '*') continue;  // comment line
        if (c0 == '+') {
            file.blocks.push_back(SinexBlock{std::string(line.substr(1)), {}});
            // Trim any leading blank the block name itself carries.
            std::string& nm = file.blocks.back().name;
            std::size_t b = nm.find_first_not_of(' ');
            nm = (b == std::string::npos) ? "" : nm.substr(b);
            current = &file.blocks.back();
            continue;
        }
        if (c0 == '-') {
            current = nullptr;
            continue;
        }
        if (c0 == ' ') {
            if (current == nullptr) {
                return odl::err(SinexError{"IOFM-F-006", "SINEX line " + std::to_string(ln) +
                    " is a data line outside any '+BLOCK'...'-BLOCK' region"});
            }
            current->lines.push_back(std::string(line.substr(1)));
            continue;
        }
        return odl::err(SinexError{"IOFM-F-006", "SINEX line " + std::to_string(ln) +
            ": first character '" + std::string(1, c0) + "' is none of %, *, +, -, or a data line's own blank"});
    }

    return file;
}

odl::Result<std::string, SinexError> write_sinex(const SinexFile& file) {
    std::ostringstream out;
    const auto& h = file.header;
    std::ostringstream ver;
    ver.setf(std::ios::fixed);
    ver.precision(2);
    ver << h.format_version;
    out << "%=SNX " << ver.str() << " " << h.file_agency_code << " " << h.creation_time << " "
        << h.data_agency_code << " " << h.data_start_time << " " << h.data_end_time << " "
        << h.observation_code << " " << h.number_of_estimates << " " << h.constraint_code;
    for (char c : h.solution_contents) out << " " << c;
    out << "\n";

    for (const auto& b : file.blocks) {
        out << "+" << b.name << "\n";
        for (const auto& l : b.lines) out << " " << l << "\n";
        out << "-" << b.name << "\n";
    }
    out << "%ENDSNX\n";
    return out.str();
}

bool operator==(const SinexHeader& a, const SinexHeader& b) noexcept {
    return std::abs(a.format_version - b.format_version) < 1e-6 && a.file_agency_code == b.file_agency_code &&
           a.creation_time == b.creation_time && a.data_agency_code == b.data_agency_code &&
           a.data_start_time == b.data_start_time && a.data_end_time == b.data_end_time &&
           a.observation_code == b.observation_code && a.number_of_estimates == b.number_of_estimates &&
           a.constraint_code == b.constraint_code && a.solution_contents == b.solution_contents;
}
bool operator==(const SinexBlock& a, const SinexBlock& b) noexcept {
    return a.name == b.name && a.lines == b.lines;
}
bool operator==(const SinexFile& a, const SinexFile& b) noexcept {
    return a.header == b.header && a.blocks == b.blocks;
}

}  // namespace odl::io
