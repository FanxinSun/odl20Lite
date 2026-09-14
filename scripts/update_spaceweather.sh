#!/bin/sh
# update_spaceweather.sh - refresh res/SW-All.csv, the solar and geomagnetic
# indices that drive the NRLMSISE-00 drag model (drag = 3).
#
# The file is CelesTrak's consolidated space weather series: observed values
# back to 1957 and predictions a few years ahead. NRLMSISE-00 uses three of its
# columns - F10.7_OBS (previous day), F10.7_OBS_CENTER81 and AP_AVG. The
# observed rather than adjusted flux is correct here: the model is calibrated
# against flux at the Earth's actual distance from the Sun, not at 1 AU.
#
# Days the file does not cover fall back to F10.7 = 150, Ap = 4, the quiet
# defaults NRLMSISE-00's own documentation gives, and the model says so once.

set -e
DEST=$(cd "$(dirname "$0")/.." && pwd)/res/SW-All.csv
URL="https://celestrak.org/SpaceData/SW-All.csv"
TMP=$(mktemp)
trap 'rm -f "$TMP"' EXIT

echo "Downloading CelesTrak space weather data ..."
if command -v curl >/dev/null 2>&1; then
    curl -fSL --retry 3 "$URL" -o "$TMP"
else
    wget -O "$TMP" "$URL"
fi

# Check the header before overwriting: the reader resolves columns by name.
for col in DATE AP_AVG F10.7_OBS F10.7_OBS_CENTER81; do
    if ! head -1 "$TMP" | tr ',' '\n' | grep -qx "$col"; then
        echo "Downloaded file has no '$col' column; refusing to overwrite $DEST" >&2
        exit 1
    fi
done

[ -f "$DEST" ] && cp "$DEST" "$DEST.bak"
cp "$TMP" "$DEST"
echo "Updated $DEST"
echo "Coverage: $(sed -n 2p "$DEST" | cut -d, -f1) to $(tail -1 "$DEST" | cut -d, -f1)"
