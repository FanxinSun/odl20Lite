# OdlManifest.cmake — the build reads the same manifest the fetcher does.
#
# Plan L0 step 3: dependency acquisition must honour URL-plus-hash pinning, not
# version-range resolution.  This is how: there is one manifest, it names a URL
# and a SHA-256, tools/fetch.py puts the bytes in the cache, and CMake consumes
# the cache and re-checks the hash independently.  No resolver sits between the
# declaration and the artefact, and no part of the build can silently acquire a
# different version.
#
# The manifest is JSON precisely so that CMake can read it with string(JSON ...)
# and no parser dependency.  A manifest format that needed a third-party parser
# to read would be a regress: the tool enforcing "nothing enters undeclared"
# would itself have entered undeclared.

include_guard(GLOBAL)
include(FetchContent)

set(ODL_MANIFEST_FILE "${CMAKE_CURRENT_LIST_DIR}/../manifest/manifest.json"
    CACHE FILEPATH "The manifest of every external input")
set(ODL_FETCH_TOOL "${CMAKE_CURRENT_LIST_DIR}/../tools/fetch.py"
    CACHE FILEPATH "The manifest fetcher")

file(READ "${ODL_MANIFEST_FILE}" _odl_manifest_json)

string(JSON _odl_schema GET "${_odl_manifest_json}" "schema")
if(NOT _odl_schema EQUAL 1)
  message(FATAL_ERROR "manifest schema ${_odl_schema} is not 1; this build speaks schema 1")
endif()
string(JSON ODL_CACHE_REL GET "${_odl_manifest_json}" "cache")
set(ODL_CACHE_DIR "${CMAKE_CURRENT_LIST_DIR}/../${ODL_CACHE_REL}")
string(JSON _odl_entry_count LENGTH "${_odl_manifest_json}" "entries")

# odl_manifest_get(<id> <out-prefix>)
#   Sets <out-prefix>_URL, _SHA256, _FILENAME, _VERSION, _LICENCE, _CACHE_PATH,
#   _UNPACKED_ROOT in the caller's scope.  Fails loudly on an unknown id: a
#   build that silently skips a dependency it cannot find is the acquisition
#   failure this step exists to prevent.
function(odl_manifest_get id out)
  math(EXPR _last "${_odl_entry_count} - 1")
  foreach(i RANGE ${_last})
    string(JSON _e GET "${_odl_manifest_json}" "entries" ${i})
    string(JSON _id GET "${_e}" "id")
    if(NOT _id STREQUAL id)
      continue()
    endif()
    string(JSON _host ERROR_VARIABLE _ignore GET "${_e}" "provided_by_host")
    if(_host)
      message(FATAL_ERROR
        "manifest entry '${id}' is provided_by_host and has no cache path; "
        "it must be located with find_program/find_package, not fetched")
    endif()
    foreach(field URL SHA256 FILENAME VERSION LICENCE)
      string(TOLOWER "${field}" _key)
      string(JSON _v ERROR_VARIABLE _err GET "${_e}" "${_key}")
      set(${out}_${field} "${_v}" PARENT_SCOPE)
      set(_local_${field} "${_v}")
    endforeach()
    string(JSON _root ERROR_VARIABLE _ignore2 GET "${_e}" "unpacked_root")
    set(${out}_UNPACKED_ROOT "${_root}" PARENT_SCOPE)
    set(${out}_CACHE_PATH "${ODL_CACHE_DIR}/${id}/${_local_FILENAME}" PARENT_SCOPE)
    return()
  endforeach()
  message(FATAL_ERROR "no manifest entry with id '${id}' in ${ODL_MANIFEST_FILE}")
endfunction()

# odl_require_cached(<id>)
#   Refuse, with the command that fixes it, rather than reaching for the network
#   behind the builder's back.  Plan §5 constraint 4 applied to the build itself.
function(odl_require_cached id)
  odl_manifest_get(${id} _m)
  if(NOT EXISTS "${_m_CACHE_PATH}")
    message(FATAL_ERROR
      "manifest entry '${id}' is not in the cache.\n"
      "  expected at  ${_m_CACHE_PATH}\n"
      "  declared as  ${_m_URL}\n"
      "\n"
      "  The build does not fetch.  Fetching is a separate, auditable act:\n"
      "      python3 ${ODL_FETCH_TOOL} fetch\n"
      "  This is deliberate — a build that downloads is a build whose inputs\n"
      "  depend on when it ran.")
  endif()
endfunction()

# odl_declare_dependency(<id>)
#   FetchContent from the LOCAL CACHE, with URL_HASH so that CMake verifies the
#   bytes a second time and independently of tools/fetch.py.  Two checks of the
#   same hash by two tools is cheap; one check is a single point of failure in
#   the one property the manifest exists to guarantee.
#
#   ORDER IS LOAD-BEARING.  FetchContent honours the FIRST declaration of a
#   given name and silently ignores every later one.  That is the supported way
#   for a top-level project to pin a transitive dependency — and it is equally
#   the way a transitive dependency captures ours if it gets in first.  It did:
#   tl::expected's own CMakeLists declares
#       FetchContent_Declare(Catch2 URL .../v2.13.10.zip)
#   with no hash, and because it was made available before our declaration, the
#   build fetched Catch2 v2.13.10 OVER THE NETWORK while reporting our pinned
#   3.16.0 from the cache.  It surfaced only because v2 puts its CMake helpers in
#   contrib/ and v3 in extras/, so an include() failed.  A substitution within a
#   major version would have built cleanly and the pin would have been a fiction.
#
#   Hence odl_declare_all_code(), called before any MakeAvailable, and hence
#   odl_verify_populated(), because relying on documented first-wins behaviour
#   without checking it is how this was missed in the first place.
function(odl_declare_dependency id)
  odl_require_cached(${id})
  odl_manifest_get(${id} _m)
  FetchContent_Declare(${id}
    URL       "${_m_CACHE_PATH}"
    URL_HASH  "SHA256=${_m_SHA256}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SYSTEM)
  message(STATUS "manifest: ${id} ${_m_VERSION} (${_m_LICENCE}) from cache, sha256 ${_m_SHA256}")
endfunction()


# odl_declare_all_code()
#   Declare every `code` entry of the manifest up front, so that this tree's
#   pins are always the first declaration and no dependency's own
#   FetchContent_Declare can win.
function(odl_declare_all_code)
  math(EXPR _last "${_odl_entry_count} - 1")
  foreach(i RANGE ${_last})
    string(JSON _e GET "${_odl_manifest_json}" "entries" ${i})
    string(JSON _kind GET "${_e}" "kind")
    string(JSON _id GET "${_e}" "id")
    if(_kind STREQUAL "code")
      odl_declare_dependency(${_id})
    endif()
  endforeach()
endfunction()

# odl_verify_populated(<id>)
#   After FetchContent_MakeAvailable, check that what landed is what was pinned.
#   The check is delegated to tools/fetch.py, which compares a file inside the
#   populated tree against the same file inside the hash-verified archive.
#   Nothing is inferred from a version string; the comparison is against bytes.
#
#   The first attempt at this compared ${<Project>_VERSION} against the manifest.
#   It does not work: project() sets that variable in the subdirectory scope
#   FetchContent adds, and it is not visible to the caller.  The check failed
#   loudly rather than passing vacuously, which is the only reason the flaw was
#   noticed — a verification that cannot see what it is verifying is worse than
#   none, because it reports success.
function(odl_verify_populated id)
  string(TOLOWER "${id}" _lc)
  if(NOT DEFINED ${_lc}_SOURCE_DIR)
    message(FATAL_ERROR
      "odl_verify_populated(${id}): ${_lc}_SOURCE_DIR is not set. "
      "Call this only after FetchContent_MakeAvailable(${id}).")
  endif()
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${ODL_FETCH_TOOL}" verify-populated ${id} "${${_lc}_SOURCE_DIR}"
    RESULT_VARIABLE _rc OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
  if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "${_out}${_err}")
  endif()
  message(STATUS "manifest: ${id} populated tree verified against the pinned archive")
endfunction()
