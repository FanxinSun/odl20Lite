# odl/self_built — top-level build.
#
# Plan L0 step 3 fixes the toolchain; §2 fixes the decomposition.  The choices,
# each recorded in manifest/manifest.json with its licence:
#
#   build system            CMake + Ninja
#   dependency acquisition  the manifest, consumed from a local cache with the
#                           hash re-checked by CMake (cmake/OdlManifest.cmake)
#   test framework          Catch2 v3 (BSL-1.0)
#
# The one thing to understand before changing anything here: THE BUILD DOES NOT
# FETCH.  A clean clone configures only after `python3 tools/fetch.py fetch` has
# run, and the failure message says so.  A build that reaches for the network is
# a build whose result depends on when it ran, which is the property this tree
# spends a whole step preventing.

cmake_minimum_required(VERSION 3.21)

project(odl_self_built
  VERSION      0.0.0
  DESCRIPTION  "A fully-owned reimplementation of the validated ODL pipeline"
  LANGUAGES    CXX C)     # C for ERFA, which is 249 flat C files with no CMake build

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)          # -std=c++20, never -std=gnu++20
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OdlManifest)
include(OdlWarnings)
include(OdlReproducible)
include(OdlModule)

find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)

# --------------------------------------------------------------------------- #
# The manifest is checked at configure time, not merely at fetch time.  A tree
# whose cache has drifted since it was populated should not build at all.

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${ODL_FETCH_TOOL}" verify
  RESULT_VARIABLE _odl_verify_rc
  OUTPUT_VARIABLE _odl_verify_out
  ERROR_VARIABLE  _odl_verify_err)
if(NOT _odl_verify_rc EQUAL 0)
  message(FATAL_ERROR
    "manifest verification failed (exit ${_odl_verify_rc}):\n${_odl_verify_out}${_odl_verify_err}")
endif()

execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${ODL_FETCH_TOOL}" check-licences
  RESULT_VARIABLE _odl_lic_rc
  OUTPUT_VARIABLE _odl_lic_out
  ERROR_VARIABLE  _odl_lic_err)
if(NOT _odl_lic_rc EQUAL 0)
  message(FATAL_ERROR
    "licence check failed — plan §5 constraint 3:\n${_odl_lic_out}${_odl_lic_err}")
endif()

# --------------------------------------------------------------------------- #
# Dependencies.
#
# EVERY manifest pin is DECLARED FIRST, before anything is made available.
# FetchContent honours the first declaration of a name and ignores later ones,
# so declaring ours up front is what stops a dependency's own unhashed
# FetchContent_Declare from capturing one of them.  It is not hypothetical:
# tl::expected declares Catch2 v2.13.10 by URL with no hash, and before this was
# reordered the build silently fetched that over the network instead of the
# pinned 3.16.0 from the cache.  See cmake/OdlManifest.cmake.

odl_declare_all_code()

# A dependency's own test suite is not this tree's business, and building it
# drags in that suite's own dependencies — which is how the capture above
# happened.
set(EXPECTED_BUILD_TESTS  OFF CACHE BOOL "" FORCE)
set(EXPECTED_BUILD_PACKAGE OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS    OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tl-expected)
odl_verify_populated(tl-expected)

# ERFA ships no CMakeLists.txt, so MakeAvailable populates it and nothing more;
# cmake/BuildErfa.cmake defines the target.  Plan §5 constraint 1.
FetchContent_MakeAvailable(erfa)
odl_verify_populated(erfa)
include(BuildErfa)

# CALCEPH.  Its own CMake project builds the C library; everything else it can
# build is turned off.  §3.11 point 4, checked before adding it: CALCEPH declares
# and fetches nothing of its own.
set(ENABLE_FORTRAN    OFF CACHE BOOL "" FORCE)
set(ENABLE_PYTHON     OFF CACHE BOOL "" FORCE)
set(ENABLE_MEX_OCTAVE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(calceph)
odl_verify_populated(calceph)

# CALCEPH generates calcephconfig.h into its own binary directory and then
# declares
#     target_include_directories(calceph PUBLIC $<BUILD_INTERFACE:>)
# — an EMPTY build interface.  Standalone that works, because the generated
# header lands in the directory the compiler is invoked from; as a subproject
# under Ninja it does not, and every translation unit fails on
# "calcephconfig.h: No such file or directory".  Repaired here rather than by
# patching the dependency, so the pinned bytes stay the pinned bytes.
#
# Third dependency in this tree whose own build system held a surprise: ERFA
# ships no CMake at all, tl::expected captured our pinned Catch2, and this.
# Plan §3.11 point 4 asks what a dependency's build system DECLARES; it is worth
# reading for what it gets wrong as well.
target_include_directories(calceph PUBLIC $<BUILD_INTERFACE:${calceph_BINARY_DIR}/src>)

include(CTest)
if(BUILD_TESTING)
  FetchContent_MakeAvailable(catch2)
  odl_verify_populated(catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  include(Catch)
endif()

# --------------------------------------------------------------------------- #
# Modules.  Each is its own target and sees only what it declares; see
# cmake/OdlModule.cmake for why that is the whole independence argument now
# that D1 has settled on C++20.
#
# L1 is the open layer: core (D7's vehicle), then time, then eop, then frames,
# one step at a time.  A module absent below is a sequence position, not an
# omission.

odl_build_erfa("${erfa_SOURCE_DIR}")

add_subdirectory(modules/core)
add_subdirectory(modules/time)
add_subdirectory(modules/eop)
add_subdirectory(modules/frames)
add_subdirectory(modules/ephemerides)

# --------------------------------------------------------------------------- #

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

message(STATUS "odl/self_built ${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}, ${CMAKE_BUILD_TYPE}")
