#!/bin/sh
# ci.sh — run every gate this tree has, from the cache, without the network.
#
# Plan L0 step 4.  The requirement is that GREEN MEANS THE FROZEN NUMBERS STILL
# HOLD, NOT THAT THE NETWORK WAS UP, so this script must be able to run with no
# network at all and must fail if anything reaches for it.
#
# Fetching is therefore NOT part of this script.  Populating the cache is a
# separate, explicit, auditable act:
#
#     tools/bootstrap.sh                  # once, online: builds tools/fetch.cpp, fetches, then runs this script
#     tools/ci.sh                         # thereafter, offline, as often as you like
#
# Anything that needs the network belongs in the first line, never the second.
# To prove the second is honest, run it with the network poisoned:
#
#     https_proxy=http://127.0.0.1:1 http_proxy=http://127.0.0.1:1 \
#     no_proxy= ALL_PROXY=http://127.0.0.1:1 tools/ci.sh
#
# which is what tools/ci.sh --prove-offline does for you.
#
# --skip-literature is for GitHub's workflow and for nothing else.  A literature entry is a provenance
# record that no build input and no test reads (plan §5 constraint 3, a checked property), and several of
# its hosts refuse GitHub's runners, so a runner cannot have the files and a red there says nothing about
# the code (PROVENANCE.md section 41.7).  With the flag, gate 1 neither requires nor verifies them -- and
# SAYS how many it left -- and so do the configure, the ctest manifest.verify_offline and the two configures
# inside gate 13, which the one environment variable ODL_SKIP_LITERATURE=1 reaches (this script sets it
# under the flag and unsets it otherwise, so a stray value in a shell cannot weaken a default run).  Without
# the flag every entry is required and verified, literature included: that is the run to make on a machine
# that has them, and the last line of a run with the flag says what that run did not cover.
#
# Usage:  ci.sh [--prove-offline] [--skip-literature] [--build-dir DIR]

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-ci"
PROVE=0
LIT_ARG=""

while [ $# -gt 0 ]; do
  case "$1" in
    --prove-offline)   PROVE=1 ;;
    --skip-literature) LIT_ARG="--skip-literature" ;;
    --build-dir)       shift; BUILD="$1" ;;
    -h|--help)         sed -n '2,/^# Usage:/p' "$0"; exit 0 ;;
    *)                 echo "ci.sh: unknown argument $1" >&2; exit 5 ;;
  esac
  shift
done

if [ "$PROVE" = "1" ]; then
  # Re-exec with every proxy variable pointed at a closed port.  Any attempt to
  # reach the network fails immediately rather than hanging, so a gate that
  # secretly depends on it fails loudly instead of passing slowly.
  echo "== re-running with the network poisoned (proxies -> 127.0.0.1:1) =="
  exec env \
    http_proxy=http://127.0.0.1:1 https_proxy=http://127.0.0.1:1 \
    HTTP_PROXY=http://127.0.0.1:1 HTTPS_PROXY=http://127.0.0.1:1 \
    ALL_PROXY=http://127.0.0.1:1 all_proxy=http://127.0.0.1:1 \
    no_proxy= NO_PROXY= \
    "$0" --build-dir "$BUILD" $LIT_ARG
fi

# The flag decides, and the environment is not trusted: under it the variable the configures read is set, otherwise it is removed.
if [ -n "$LIT_ARG" ]; then
  ODL_SKIP_LITERATURE=1
  export ODL_SKIP_LITERATURE
  echo "== --skip-literature: literature entries are NOT required or verified in this run (GitHub's runner; every other run does) =="
else
  unset ODL_SKIP_LITERATURE
fi

PY=${PYTHON:-python3}
step=0
gate() { step=$((step + 1)); printf '\n== gate %d: %s ==\n' "$step" "$1"; }

cd "$ROOT"

# The manifest tool is C++ (tools/fetch.cpp) and runs before any configure, so it is compiled here with the compiler the configure will use: the
# same helper, the same flags and the same output directory as the configure's own call (cmake/OdlBuildHostTool.cmake), which then finds it built.
HOST="$BUILD/host"
cmake -DODL_HOST_OUT="$HOST" -DODL_CXX="${CXX:-c++}" -P "$ROOT/cmake/OdlBuildHostTool.cmake"

if [ -n "$LIT_ARG" ]; then
  gate "manifest verifies offline (--skip-literature: literature entries left to local verification)"
else
  gate "manifest verifies offline"
fi
"$HOST/fetch" verify $LIT_ARG

gate "every dependency licence is permissive (plan §5 constraint 3)"
"$HOST/fetch" check-licences

gate "configure"
cmake -S . -B "$BUILD" -G Ninja -DODL_WERROR=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

gate "build"
cmake --build "$BUILD"

gate "test"
ctest --test-dir "$BUILD" --output-on-failure

gate "NOTICE regenerates and matches what is committed"
"$BUILD/tools/notice" --check

gate "specification traceability (NOT a test-suite check — see the tool's output)"
"$PY" tools/speccheck.py

gate "budget-row arithmetic (specifications' only untested numbers)"
"$PY" tools/budgetcheck.py --quiet

gate "plan §5 constraint 8 — odl::Result only, no monadic chaining"
"$PY" tools/constraint8.py

gate "RKF7(8)'s tableau satisfies the order conditions exactly (SPEC-integrators INTG-A-001)"
"$PY" tools/rk_coefficients.py --check

gate "no build input can reach a literature entry (plan §5 constraint 3)"
"$PY" tools/literaturecheck.py --quiet

gate "every factor of a thousand is accounted for (SPEC-dynamics DYN-R-040)"
"$PY" tools/unitcheck.py --quiet

gate "build is reproducible"
"$PY" tools/reprocheck.py --build-dir "$BUILD"

printf '\n== all %d gates passed ==\n' "$step"
if [ -n "$LIT_ARG" ]; then
  printf '== BUT with --skip-literature: the literature entries were NOT required or verified in this run (gate 1 says how many). Run tools/ci.sh without it, on a machine that has them. ==\n'
fi
