# OdlBuildHostTool.cmake — compile the one tool that has to exist before the build can.
#
# Plan L0 step 8.  Configure verifies the manifest before it makes anything available (`fetch verify`, `check-licences`, and
# `verify-populated` after each FetchContent_MakeAvailable), and `tools/ci.sh` runs the first two before any configure at all.  The tool is
# C++, and the build cannot build C++ before it has verified what it is about to build from: so this compiles tools/fetch.cpp with the
# configured compiler, once, into <build>/host/fetch.  It is the same source the tests compile again under the tree's strict warnings
# (tools/CMakeLists.txt), and the ONE implementation of the manifest's rules.  Rejected: the same rules written a second time in CMake script,
# a language in which the licence allowlist and the archive rules could not be tested the way the C++ is.
#
# Two ways in:
#
#   include(OdlBuildHostTool)  odl_build_host_tool()                     # from CMakeLists.txt: the configured compiler; sets ODL_FETCH_TOOL
#   cmake -DODL_HOST_OUT=<dir> [-DODL_CXX=<compiler>] -P cmake/OdlBuildHostTool.cmake
#                                                                         # from ci.sh, bootstrap.sh and the workflow, before any configure
#
# The compile is skipped when the binary is newer than every source and header and was built by the same command, so a second call costs
# nothing.  The flags carry the tree's own -ffile-prefix-map (see OdlReproducible.cmake): tools/reprocheck compares every executable of the
# build directory, and this one lives in it.

include_guard(GLOBAL)

function(odl_build_host_tool)
  get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)

  if(NOT ODL_HOST_OUT)
    if(CMAKE_BINARY_DIR AND NOT CMAKE_SCRIPT_MODE_FILE)
      set(ODL_HOST_OUT "${CMAKE_BINARY_DIR}/host")
    else()
      message(FATAL_ERROR "odl_build_host_tool: say where to put it: -DODL_HOST_OUT=<dir>")
    endif()
  endif()
  get_filename_component(_out "${ODL_HOST_OUT}" ABSOLUTE)

  if(ODL_CXX)
    set(_cxx "${ODL_CXX}")
  elseif(CMAKE_CXX_COMPILER AND NOT CMAKE_SCRIPT_MODE_FILE)
    set(_cxx "${CMAKE_CXX_COMPILER}")
  elseif(DEFINED ENV{CXX} AND NOT "$ENV{CXX}" STREQUAL "")
    set(_cxx "$ENV{CXX}")
  else()
    set(_cxx "c++")
  endif()
  # ci.sh says `c++`, the configure says `/usr/bin/c++`: the same compiler, and the second call must find the first one's tool built.
  find_program(_cxx_resolved NAMES "${_cxx}" NO_CACHE)
  if(_cxx_resolved)
    set(_cxx "${_cxx_resolved}")
  endif()

  set(_sources
    tools/fetch.cpp
    tools/devkit/src/tool.cpp tools/devkit/src/text.cpp tools/devkit/src/pyfmt.cpp tools/devkit/src/sha256.cpp tools/devkit/src/json.cpp
    tools/devkit/src/inflate.cpp tools/devkit/src/archive.cpp tools/devkit/src/fs.cpp tools/devkit/src/process.cpp)
  list(TRANSFORM _sources PREPEND "${_root}/")
  file(GLOB_RECURSE _headers "${_root}/tools/devkit/include/*.hpp")
  list(APPEND _headers "${_root}/tools/fetch.hpp")

  set(_exe "${_out}/fetch")
  set(_flags
    -std=c++20 -O1 -g0 -fno-ident -ffp-contract=off
    "-ffile-prefix-map=${_root}=." "-ffile-prefix-map=${_out}=."     # the more specific map LAST: gcc tries them last-first
    "-I${_root}/tools/devkit/include" "-I${_root}/tools"
    "-DODL_TREE_ROOT=\"${_root}\"")
  string(JOIN " " _command "${_cxx}" ${_flags} ${_sources})

  set(_stamp "${_out}/fetch.command")
  set(_stale FALSE)
  if(NOT EXISTS "${_exe}" OR NOT EXISTS "${_stamp}")
    set(_stale TRUE)
  else()
    file(READ "${_stamp}" _previous)
    if(NOT _previous STREQUAL _command)
      set(_stale TRUE)
    else()
      foreach(_input IN LISTS _sources _headers)
        if("${_input}" IS_NEWER_THAN "${_exe}")
          set(_stale TRUE)
          break()
        endif()
      endforeach()
    endif()
  endif()

  if(_stale)
    file(MAKE_DIRECTORY "${_out}")
    execute_process(
      COMMAND "${_cxx}" ${_flags} ${_sources} -o "${_exe}.tmp"
      RESULT_VARIABLE _rc OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr)
    if(NOT _rc EQUAL 0)
      file(REMOVE "${_exe}.tmp")
      message(FATAL_ERROR
        "the manifest tool (tools/fetch.cpp) did not compile with ${_cxx} (${_rc}):\n${_stdout}${_stderr}\n"
        "  It is built before anything else, because the build verifies its inputs with it.")
    endif()
    file(RENAME "${_exe}.tmp" "${_exe}")
    file(WRITE "${_stamp}" "${_command}")
    message(STATUS "host tool: ${_exe} (built with ${_cxx})")
  endif()

  set(ODL_FETCH_TOOL "${_exe}" CACHE FILEPATH "The manifest fetcher, built at configure time (cmake/OdlBuildHostTool.cmake)" FORCE)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
  odl_build_host_tool()
endif()
