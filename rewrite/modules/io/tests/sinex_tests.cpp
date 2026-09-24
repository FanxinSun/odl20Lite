// sinex_tests.cpp — SPEC-io-formats.md §8, IOFM-A-011/013.

#include <catch2/catch_test_macros.hpp>

#include <odl/io/sinex.hpp>

using namespace odl;
using namespace odl::io;

namespace {

/// A minimal synthetic file built strictly to SINEX2 §2/§3's own stated
/// grammar (header line, one FILE/REFERENCE block, footer) -- SPEC-io-formats.md
/// `IOFM-A-011`'s own stated fixture, since `SINEX2` itself does not print a
/// short complete-file example this reader's own scope (the general
/// container, not any one block's fields) would gain from reproducing.
const char* kSample =
"%=SNX 2.02 IGS 26:015:12345 IGS 00:000:00000 00:000:00000 P 00042 2 S O\n"
"*-------------------------------------------------------------------------\n"
"+FILE/REFERENCE\n"
" DESCRIPTION        A synthetic test SINEX file\n"
" OUTPUT              Combined station position and EOP solution\n"
" CONTACT              analyst@example.org\n"
"-FILE/REFERENCE\n"
"%ENDSNX\n";

}  // namespace

TEST_CASE("IOFM-A-011  read_sinex on a grammar-conforming synthetic file "
          "recovers the header fields and the one block's own lines "
          "verbatim; an unrecognised leading character refuses IOFM-F-006",
          "[io][sinex]") {
    auto f = read_sinex(kSample);
    REQUIRE(f.has_value());
    CHECK(f->header.format_version == 2.02);
    CHECK(f->header.file_agency_code == "IGS");
    CHECK(f->header.creation_time == "26:015:12345");
    CHECK(f->header.observation_code == 'P');
    CHECK(f->header.number_of_estimates == 42);
    CHECK(f->header.constraint_code == '2');
    CHECK(f->header.solution_contents == "SO");

    REQUIRE(f->blocks.size() == 1);
    CHECK(f->blocks[0].name == "FILE/REFERENCE");
    REQUIRE(f->blocks[0].lines.size() == 3);
    CHECK(f->blocks[0].lines[0] == "DESCRIPTION        A synthetic test SINEX file");

    // A line appended after %ENDSNX would never be reached (the reader stops
    // the scan there) -- inject the bad line BEFORE the footer instead.
    std::string with_bad = kSample;
    auto pos = with_bad.find("%ENDSNX");
    with_bad.insert(pos, "?not a valid line\n");
    auto bad2 = read_sinex(with_bad);
    REQUIRE_FALSE(bad2.has_value());
    CHECK(bad2.error().id == "IOFM-F-006");
}

TEST_CASE("IOFM-A-013g  SINEX round-trips: read(write(read(fixture))) == read(fixture)",
          "[io][sinex][gate]") {
    auto f1 = read_sinex(kSample);
    REQUIRE(f1.has_value());
    auto text2 = write_sinex(*f1);
    REQUIRE(text2.has_value());
    auto f2 = read_sinex(*text2);
    REQUIRE(f2.has_value());
    CHECK(*f1 == *f2);
}
