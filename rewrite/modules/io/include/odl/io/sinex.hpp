#pragma once
// odl/io/sinex.hpp — SINEX, at the general block-structure level.
//
// SPEC-io-formats.md §3.6. Source: IGS/IERS/ILRS/IVS SINEX Working Group,
// "SINEX -- Solution (Software/technique) INdependent EXchange Format",
// Version 2.02, 1 December 2006 (`SINEX2`). This reader parses the general
// container -- the header line and every `+BLOCK`...`-BLOCK` region, held
// verbatim -- not any one block's own field semantics (SPEC-io-formats.md
// §1/§3.6's own stated scope: a caller reads a specific block's own fields
// against `SINEX2` directly, the way `IGSMETA`'s `SATELLITE/MASS` field was
// already read at L5, ad hoc, before this general reader existed).

#include <odl/core/result.hpp>

#include <string>
#include <vector>

namespace odl::io {

using SinexError = odl::Diagnostic;

/// `SINEX2` §3's own header line fields, in order. Every field is single-
/// blank-delimited in a real file (the FORTRAN "1X" separators between
/// fixed-column groups never fall inside a value: agency/observation/
/// constraint codes are non-blank single tokens, and the three [Time] fields
/// use ':' internally, never a space) -- so this reader tokenises on
/// whitespace rather than extracting literal columns, recovering the exact
/// same fields the fixed-column format specifies without the fixed-column
/// transcription risk a byte-exact column read would carry.
struct SinexHeader {
    double format_version = 0.0;
    std::string file_agency_code;
    std::string creation_time;    ///< "YY:DDD:SSSSS", raw (not decoded to an Epoch here -- no LeapTable, §3.1)
    std::string data_agency_code;
    std::string data_start_time;  ///< "YY:DDD:SSSSS" or "00:000:00000" if unstated
    std::string data_end_time;
    char observation_code = ' ';
    int number_of_estimates = 0;
    char constraint_code = ' ';
    std::string solution_contents;  ///< up to 6 one-character codes, concatenated, spaces where blank
};

/// One `+NAME`...`-NAME` region (`SINEX2` §2): every data line held verbatim,
/// the block's own leading single space (the format's own "data line" marker)
/// stripped, not re-parsed into fields -- a specific block's own field
/// layout is this reader's own stated non-goal (see this file's own header).
struct SinexBlock {
    std::string name;
    std::vector<std::string> lines;
};

struct SinexFile {
    SinexHeader header;
    std::vector<SinexBlock> blocks;
};

[[nodiscard]] odl::Result<SinexFile, SinexError> read_sinex(std::string_view text);
[[nodiscard]] odl::Result<std::string, SinexError> write_sinex(const SinexFile& file);

[[nodiscard]] bool operator==(const SinexHeader&, const SinexHeader&) noexcept;
[[nodiscard]] bool operator==(const SinexBlock&, const SinexBlock&) noexcept;
[[nodiscard]] bool operator==(const SinexFile&, const SinexFile&) noexcept;

}  // namespace odl::io
