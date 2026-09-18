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
  LANGUAGES    CXX)

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
# Modules.  Each is its own target and sees only what it declares; see
# cmake/OdlModule.cmake for why that is the whole independence argument now
# that D1 has settled on C++20.
#
# None exist yet: L1 (time, eop, frames) is the next layer to open and does not
# open until L0's exit gate passes.  The absence is stated rather than left
# blank so that it reads as a sequence position, not an omission.

# --------------------------------------------------------------------------- #

include(CTest)
if(BUILD_TESTING)
  odl_declare_dependency(catch2)
  FetchContent_MakeAvailable(catch2)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  include(Catch)
  add_subdirectory(tests)
endif()

message(STATUS "odl/self_built ${PROJECT_VERSION} — C++${CMAKE_CXX_STANDARD}, ${CMAKE_BUILD_TYPE}")
