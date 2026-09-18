# OdlModule.cmake — the §2 layering, made a link boundary rather than a habit.
#
# WHY THIS EXISTS, and it is the substance of the independence argument now that
# D1 has chosen C++20.  The Rust case for D1 rested partly on structural distance
# from a GNU-C++14 predecessor.  With the same language and the same problem, the
# decomposition is the only thing left that distinguishes the two trees — so §2's
# layering has to be enforced, not merely described.
#
# In C++ a directory is not a boundary.  A link target is.  Every module of §2 is
# therefore its own CMake target with its own PUBLIC include directory, and it
# can see exactly the modules named in DEPENDS and nothing else.  A module that
# reaches across the layering fails to compile, which is what Rust's crate
# boundaries would have given for free and what a flat src/ + include/ tree
# would have given up.
#
#   odl_add_module(time
#     SOURCES  src/epoch.cpp src/leap_table.cpp
#     DEPENDS  )                       # the floor: depends on nothing
#
#   odl_add_module(frames
#     SOURCES  src/rotation.cpp
#     DEPENDS  odl::time odl::eop)     # and NOT odl::env — it cannot see it
#
# Layout, one directory per module, headers beside the sources that implement
# them so that a module is a thing you can pick up whole:
#
#   modules/<name>/include/odl/<name>/*.hpp     PUBLIC  — the module's surface
#   modules/<name>/src/*.{hpp,cpp}              PRIVATE — its internals
#   modules/<name>/tests/*.cpp                          — its own acceptance suite
#
# Note the absence of lib/, bin/, obj/ and output/ as directory names anywhere in
# this tree.  The repository root's .gitignore carries unanchored patterns for
# all four, so a directory of any of those names under rewrite/ would be silently
# untracked.  Naming around it is cheaper than fighting it.

include_guard(GLOBAL)

define_property(GLOBAL PROPERTY ODL_MODULES
  BRIEF_DOCS "Every module declared with odl_add_module")

function(odl_add_module name)
  cmake_parse_arguments(A "" "" "SOURCES;DEPENDS;TEST_SOURCES" ${ARGN})
  if(A_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "odl_add_module(${name}): unexpected arguments: ${A_UNPARSED_ARGUMENTS}")
  endif()

  set(_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  set(_inc "${_dir}/include")
  if(NOT IS_DIRECTORY "${_inc}")
    message(FATAL_ERROR
      "odl_add_module(${name}): no include/ directory at ${_inc}.\n"
      "  A module's public surface is modules/${name}/include/odl/${name}/.\n"
      "  A module with no public surface is not a module.")
  endif()

  if(A_SOURCES)
    add_library(odl_${name} STATIC ${A_SOURCES})
  else()
    add_library(odl_${name} INTERFACE)
  endif()
  add_library(odl::${name} ALIAS odl_${name})

  if(A_SOURCES)
    target_include_directories(odl_${name} PUBLIC "${_inc}" PRIVATE "${_dir}/src")
    target_link_libraries(odl_${name} PUBLIC ${A_DEPENDS} PRIVATE odl_warnings)
    target_compile_features(odl_${name} PUBLIC cxx_std_20)
  else()
    target_include_directories(odl_${name} INTERFACE "${_inc}")
    target_link_libraries(odl_${name} INTERFACE ${A_DEPENDS})
    target_compile_features(odl_${name} INTERFACE cxx_std_20)
  endif()

  set_property(GLOBAL APPEND PROPERTY ODL_MODULES ${name})

  if(A_TEST_SOURCES AND BUILD_TESTING)
    add_executable(odl_${name}_tests ${A_TEST_SOURCES})
    target_link_libraries(odl_${name}_tests
      PRIVATE odl::${name} Catch2::Catch2WithMain odl_warnings)
    catch_discover_tests(odl_${name}_tests)
  endif()
endfunction()
