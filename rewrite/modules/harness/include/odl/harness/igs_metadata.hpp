#pragma once
// odl/harness/igs_metadata.hpp — the IGS satellite metadata SINEX (manifest entry `igs-satellite-metadata`, release 2446 at the pin): which SVN carried a PRN at an epoch, and its block.
//
// Rules (plan/subplan_L8/L8-1.md, registered 2026-11-27): the PRN of a satellite at an epoch is the ONE `+SATELLITE/PRN` row with Valid_From <= t < Valid_To (`0000:000:00000` = open); no row, or more than one, is a
// refusal with NO fallback to "the latest"; the block is the `Block` field of `+SATELLITE/IDENTIFIER` for that SVN, and a missing identifier is a refusal.
#include <odl/core/result.hpp>
#include <odl/harness/statistics.hpp>
#include <odl/time/epoch.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace odl::harness {

/// A SINEX epoch `YYYY:DDD:SSSSS` (year, day of year, seconds of the day).
struct SinexEpoch {
    int year = 0;
    int day_of_year = 0;
    int seconds = 0;
    [[nodiscard]] static SinexEpoch from_calendar(const odl::time::Calendar& c);
    [[nodiscard]] bool open() const noexcept { return year == 0 && day_of_year == 0 && seconds == 0; }
    friend bool operator<(const SinexEpoch& a, const SinexEpoch& b) noexcept {
        if (a.year != b.year) return a.year < b.year;
        if (a.day_of_year != b.day_of_year) return a.day_of_year < b.day_of_year;
        return a.seconds < b.seconds;
    }
};

struct IgsMetadataEntry {
    std::string prn;                    ///< "G01"
    std::string svn;                    ///< "G063"
    std::string block;                  ///< "GPS-IIF"
    SinexEpoch valid_from, valid_to;    ///< valid_to open() = no end
};

class IgsSatelliteMetadata {
public:
    /// Parses the `+SATELLITE/IDENTIFIER` and `+SATELLITE/PRN` blocks.  Refuses (HARN-F-010) a text with neither block, or a row that does not read.
    [[nodiscard]] static odl::Result<IgsSatelliteMetadata, HarnessError> parse(std::string_view sinex_text);

    /// The one entry whose interval holds `epoch` for `prn`, with its block.  Refuses (HARN-F-011) zero or more than one interval, (HARN-F-012) a missing identifier.
    [[nodiscard]] odl::Result<IgsMetadataEntry, HarnessError> resolve_prn(const std::string& prn, const SinexEpoch& epoch) const;

    [[nodiscard]] std::size_t prn_rows() const noexcept { return rows_.size(); }
    [[nodiscard]] std::size_t identifier_rows() const noexcept { return blocks_.size(); }

private:
    std::vector<IgsMetadataEntry> rows_;                          ///< PRN rows, block empty until resolved
    std::vector<std::pair<std::string, std::string>> blocks_;     ///< (SVN, block)
};

}  // namespace odl::harness
