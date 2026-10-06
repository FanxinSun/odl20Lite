#!/bin/sh
# bootstrap.sh — a clean clone to a green gate, in one command.
#
# L0's exit gate asks that a clean clone build, test and regenerate NOTICE with
# one command.  Step 4 asks that green mean the frozen numbers still hold and not
# that the network was up.  Those pull in opposite directions, and this is how
# they are both honoured:
#
#     tools/bootstrap.sh    = fetch (online, once)  then  ci.sh (offline)
#     tools/ci.sh           = the gates alone, and they must pass with no network
#
# So a newcomer runs one command, and the gate still proves what it claims —
# because the network phase is a separate, visible, auditable act rather than a
# silent prelude folded into every build.

set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
HOST="$ROOT/build-ci/host"

echo "== building the manifest tool, tools/fetch.cpp (the build cannot build C++ before it has verified what it builds from) =="
cmake -DODL_HOST_OUT="$HOST" -DODL_CXX="${CXX:-c++}" -P "$ROOT/cmake/OdlBuildHostTool.cmake"

echo
echo "== populating the manifest cache from origin (the only online step) =="
"$HOST/fetch" fetch

echo
echo "== every gate, offline =="
exec "$ROOT/tools/ci.sh" --prove-offline "$@"
