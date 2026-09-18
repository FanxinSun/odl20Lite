# OdlWarnings.cmake — one interface target carrying the diagnostic settings.
#
# The predecessor's expensive defect class was the plausible wrong number, and a
# good share of that class is visible to a compiler that is asked to look:
# narrowing conversions, sign mismatches in comparisons, uninitialised reads,
# a discarded [[nodiscard]] result.  That last one matters more here than
# anywhere: the adopted specifications make every fallible operation return a
# diagnostic the caller must consume (SPEC-template §5 R-ERR-1), and in C++ the
# only thing standing between that contract and a silently dropped error is
# -Wunused-result being an error.

include_guard(GLOBAL)

add_library(odl_warnings INTERFACE)
add_library(odl::warnings ALIAS odl_warnings)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_warnings INTERFACE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion
    -Wshadow -Wnon-virtual-dtor -Wold-style-cast
    -Wcast-align -Wunused -Woverloaded-virtual
    -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    -Wnull-dereference
    # Discarding a diagnostic is not a warning in this tree; it is an error.
    -Werror=unused-result
    -Werror=return-type)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(odl_warnings INTERFACE -Wduplicated-cond -Wlogical-op -Wuseless-cast)
  endif()
elseif(MSVC)
  target_compile_options(odl_warnings INTERFACE /W4 /permissive-)
endif()

option(ODL_WERROR "Treat all warnings as errors (on in CI)" OFF)
if(ODL_WERROR)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(odl_warnings INTERFACE -Werror)
  elseif(MSVC)
    target_compile_options(odl_warnings INTERFACE /WX)
  endif()
endif()
