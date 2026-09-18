# OdlReproducible.cmake — plan L0 step 7.
#
# A reproducible build is not a nicety here.  This tree's whole claim is that a
# stated input produces a stated number, and rule R11 already says the identity
# of an input is its hash.  A binary whose hash depends on which directory it was
# built in, or on what time it was built at, breaks the same chain one link
# further along: the manifest can prove which bytes went in, and without this the
# tree cannot prove which bytes came out.
#
# What makes a GCC/Clang C++ build non-reproducible, and what is done about each:
#
#   absolute paths baked into debug info, __FILE__, and assert() strings
#       -ffile-prefix-map, mapping both the source and the build tree to '.'.
#       ORDER MATTERS AND IS COUNTER-INTUITIVE: GCC tries these LAST-SPECIFIED
#       FIRST, so the more specific map must be written last.  The build tree
#       normally lives inside the source tree, so with the source map tried
#       first, DW_AT_comp_dir — which is the compiler's working directory, i.e.
#       the build tree — comes out as './build-whatever' and the build directory's
#       name leaks into every object file.  Measured, not assumed: the first
#       draft had them the other way round and reprocheck.py caught it, with the
#       artefact sizes differing by exactly the difference in directory-name
#       length.
#
#   __DATE__, __TIME__, __TIMESTAMP__
#       -Werror=date-time turns any use into a compile error.  Nothing in this
#       tree needs to know when it was built; anything that did would be reading
#       a wall clock into a result, which §5 constraint 5 already forbids in
#       spirit.
#
#   the compiler's own identification string in .comment
#       -fno-ident.  It varies with the packaging of the compiler, not with the
#       source, so it is noise in an artefact hash.
#
#   archive member timestamps, uid/gid and mode
#       ar 'D' (deterministic) and ranlib -D, set explicitly rather than relying
#       on the distribution's default, which differs between distributions.
#
#   SOURCE_DATE_EPOCH
#       honoured by GCC where a date is genuinely unavoidable.  ci.sh pins it.
#
# tools/reprocheck.py proves the result by building twice into directories of
# DIFFERENT NAME LENGTHS and comparing artefact hashes — different lengths
# because an equal-length path can hide a leak that a differing one exposes.

include_guard(GLOBAL)

option(ODL_REPRODUCIBLE "Build reproducibly: no paths, no timestamps, no compiler ident" ON)

if(NOT ODL_REPRODUCIBLE)
  message(STATUS "reproducible build: OFF")
  return()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."   # tried second
    "-ffile-prefix-map=${CMAKE_BINARY_DIR}=."   # tried FIRST — see the note above
    -fno-ident)
  # Applied to this tree's own code only.  Third-party sources fetched by the
  # manifest are compiled as they come; policing their macro use is not this
  # tree's business, and a dependency that used __DATE__ would show up in
  # reprocheck.py as a differing artefact, which is the check that matters.
  target_compile_options(odl_warnings INTERFACE -Werror=date-time)

  set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcD <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_APPEND "<CMAKE_AR> qD  <TARGET> <LINK_FLAGS> <OBJECTS>")
  set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> -D <TARGET>")
endif()

message(STATUS "reproducible build: ON (file-prefix-map, no ident, deterministic ar)")
