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
PY=${PYTHON:-python3}

echo "== populating the manifest cache from origin (the only online step) =="
"$PY" "$ROOT/tools/fetch.py" fetch

echo
echo "== every gate, offline =="
exec "$ROOT/tools/ci.sh" --prove-offline "$@"
