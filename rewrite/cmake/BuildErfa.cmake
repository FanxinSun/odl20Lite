# BuildErfa.cmake — build ERFA, which ships no CMake build of its own.
#
# ERFA v2.0.1 is autotools only: `configure.ac`, `Makefile.am`, no CMakeLists.txt
# anywhere.  So FetchContent populates it and this file builds it, rather than
# adding its subdirectory.  The alternative — running its configure script from
# CMake — would make the build depend on autotools being present and on a
# generated `config.h` whose contents vary with the host, which is a poor trade
# for a library that is 251 flat C files with no configuration to speak of.
#
# The version macros are the one thing `config.h` would have supplied
# (`erfaversion.c` reads PACKAGE_VERSION and SOFA_VERSION), so they are passed on
# the command line.  Their values come from the pinned archive's own
# `configure.ac`: AC_INIT([erfa],[2.0.1]) and SOFA_VERSION "20231011".
#
# ERFA's own test programme is built and registered with CTest.  That is not
# diligence for its own sake: SPEC-frames.md `FRAME-A-004` names ERFA's published
# verification values as the source of truth for the transformation chain, so
# running them here is the spec's acceptance test, already written by ERFA.

include_guard(GLOBAL)

function(odl_build_erfa src_dir)
  file(GLOB _erfa_sources CONFIGURE_DEPENDS "${src_dir}/src/*.c")
  list(FILTER _erfa_sources EXCLUDE REGEX "/t_erfa_c(_extra)?\\.c$")
  list(LENGTH _erfa_sources _n)
  if(_n LESS 200)
    message(FATAL_ERROR
      "ERFA source glob found only ${_n} files in ${src_dir}/src — expected ~251. "
      "The populated tree is not the archive this manifest pins.")
  endif()

  add_library(erfa STATIC ${_erfa_sources})
  add_library(ERFA::erfa ALIAS erfa)
  target_include_directories(erfa SYSTEM PUBLIC "${src_dir}/src")
  target_compile_definitions(erfa PRIVATE
    PACKAGE_VERSION="2.0.1"
    PACKAGE_VERSION_MAJOR=2
    PACKAGE_VERSION_MINOR=0
    PACKAGE_VERSION_MICRO=1
    SOFA_VERSION="20231011")
  set_target_properties(erfa PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
  # ERFA is trigonometry from end to end; on platforms where libm is separate it
  # must be linked, and PUBLIC because consumers link erfa statically.
  include(CheckLibraryExists)
  check_library_exists(m sincos "" ODL_HAVE_LIBM)
  if(ODL_HAVE_LIBM)
    target_link_libraries(erfa PUBLIC m)
  endif()
  # Third-party C, compiled as it comes: this tree's warning settings are about
  # this tree's code, and -Wconversion on 251 files of translated Fortran would
  # be noise that trains people to ignore warnings.
  message(STATUS "erfa: ${_n} translation units from ${src_dir}/src")

  if(BUILD_TESTING AND EXISTS "${src_dir}/src/t_erfa_c.c")
    add_executable(erfa_selftest "${src_dir}/src/t_erfa_c.c")
    target_link_libraries(erfa_selftest PRIVATE erfa)
    add_test(NAME erfa.published_verification_values COMMAND erfa_selftest)
    set_tests_properties(erfa.published_verification_values PROPERTIES
      PASS_REGULAR_EXPRESSION "t_erfa_c validation successful")
  endif()
endfunction()
