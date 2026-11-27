#include <odl/harness/igs_metadata.hpp>

#include <array>
#include <cctype>
#include <charconv>

namespace odl::harness {

namespace {

constexpr std::array<int, 12> kDaysBefore{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

[[nodiscard]] bool leap_year(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

std::string trim(std::string_view s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

[[nodiscard]] bool read_int(std::string_view s, int& out) {
    const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

/// `YYYY:DDD:SSSSS`
[[nodiscard]] bool read_epoch(std::string_view s, SinexEpoch& e) {
    if (s.size() != 14 || s[4] != ':' || s[8] != ':') return false;
    return read_int(s.substr(0, 4), e.year) && read_int(s.substr(5, 3), e.day_of_year) && read_int(s.substr(9, 5), e.seconds);
}

}  // namespace

SinexEpoch SinexEpoch::from_calendar(const odl::time::Calendar& c) {
    SinexEpoch e;
    e.year = c.year;
    const int month_index = c.month >= 1 && c.month <= 12 ? c.month - 1 : 0;
    e.day_of_year = kDaysBefore[static_cast<std::size_t>(month_index)] + c.day + ((c.month > 2 && leap_year(c.year)) ? 1 : 0);
    e.seconds = c.hour * 3600 + c.minute * 60 + static_cast<int>(c.second);
    return e;
}

odl::Result<IgsSatelliteMetadata, HarnessError> IgsSatelliteMetadata::parse(std::string_view text) {
    IgsSatelliteMetadata m;
    enum class Block { none, identifier, prn } where = Block::none;
    std::size_t pos = 0;
    while (pos <= text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        const std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.rfind("+SATELLITE/IDENTIFIER", 0) == 0) { where = Block::identifier; continue; }
        if (line.rfind("+SATELLITE/PRN", 0) == 0) { where = Block::prn; continue; }
        if (!line.empty() && line[0] == '-') { where = Block::none; continue; }
        if (where == Block::none || line.empty() || line[0] != ' ') continue;
        if (where == Block::identifier) {
            // " G063 2011-036A  37753 GPS-IIF         Launched …": SVN columns 2-5, Block columns 24-38
            if (line.size() < 24) return odl::err(HarnessError{"HARN-F-010", "SATELLITE/IDENTIFIER row too short: " + std::string(line)});
            m.blocks_.emplace_back(trim(line.substr(1, 4)), trim(line.substr(23, 15)));
        } else {
            // " G063 2011:197:00000 2024:103:00000 G01 ": SVN 2-5, Valid_From 7-20, Valid_To 22-35, PRN 37-39
            if (line.size() < 39) return odl::err(HarnessError{"HARN-F-010", "SATELLITE/PRN row too short: " + std::string(line)});
            IgsMetadataEntry e;
            e.svn = trim(line.substr(1, 4));
            e.prn = trim(line.substr(36, 3));
            if (!read_epoch(line.substr(6, 14), e.valid_from) || !read_epoch(line.substr(21, 14), e.valid_to)) return odl::err(HarnessError{"HARN-F-010", "SATELLITE/PRN row with an unreadable interval: " + std::string(line)});
            m.rows_.push_back(std::move(e));
        }
    }
    if (m.rows_.empty() || m.blocks_.empty()) return odl::err(HarnessError{"HARN-F-010", "the text has no SATELLITE/PRN or no SATELLITE/IDENTIFIER rows"});
    return m;
}

odl::Result<IgsMetadataEntry, HarnessError> IgsSatelliteMetadata::resolve_prn(const std::string& prn, const SinexEpoch& epoch) const {
    const IgsMetadataEntry* found = nullptr;
    std::size_t count = 0;
    for (const IgsMetadataEntry& e : rows_) {
        if (e.prn != prn) continue;
        if (epoch < e.valid_from) continue;
        if (!e.valid_to.open() && !(epoch < e.valid_to)) continue;
        found = &e;
        ++count;
    }
    if (count != 1) return odl::err(HarnessError{"HARN-F-011", "PRN " + prn + " has " + std::to_string(count) + " intervals at the epoch (exactly one is required; there is no fallback)"});
    IgsMetadataEntry out = *found;
    for (const auto& [svn, block] : blocks_)
        if (svn == out.svn) { out.block = block; return out; }
    return odl::err(HarnessError{"HARN-F-012", "SVN " + out.svn + " has no SATELLITE/IDENTIFIER row"});
}

}  // namespace odl::harness
