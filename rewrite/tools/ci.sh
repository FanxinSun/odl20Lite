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
#     python3 tools/fetch.py fetch        # once, online
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
# Usage:  ci.sh [--prove-offline] [--build-dir DIR]

set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build-ci"
PROVE=0

while [ $# -gt 0 ]; do
  case "$1" in
    --prove-offline) PROVE=1 ;;
    --build-dir)     shift; BUILD="$1" ;;
    -h|--help)       sed -n '2,25p' "$0"; exit 0 ;;
    *)               echo "ci.sh: unknown argument $1" >&2; exit 5 ;;
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
    "$0" --build-dir "$BUILD"
fi

PY=${PYTHON:-python3}
step=0
gate() { step=$((step + 1)); printf '\n== gate %d: %s ==\n' "$step" "$1"; }

cd "$ROOT"

gate "manifest verifies offline"
"$PY" tools/fetch.py verify

gate "every dependency licence is permissive (plan §5 constraint 3)"
"$PY" tools/fetch.py check-licences

gate "configure"
cmake -S . -B "$BUILD" -G Ninja -DODL_WERROR=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo

gate "build"
cmake --build "$BUILD"

gate "test"
ctest --test-dir "$BUILD" --output-on-failure

gate "NOTICE regenerates and matches what is committed"
"$PY" tools/notice.py --check

gate "specification coverage"
"$PY" tools/speccheck.py

gate "plan §5 constraint 8 — odl::Result only, no monadic chaining"
"$PY" tools/constraint8.py

gate "build is reproducible"
"$PY" tools/reprocheck.py --build-dir "$BUILD"

printf '\n== all %d gates passed ==\n' "$step"
