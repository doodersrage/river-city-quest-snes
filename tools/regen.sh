#!/bin/sh
# Re-export the web game's art and convert it to SNES data.
# Usage: tools/regen.sh <path-to-river-city-quest>
set -e
WEB=$(cd "$1" && pwd)
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE/tools/export"
[ -d node_modules ] || npm install
VARIANTS=4 node export.js "$WEB" "$HERE/build/export"
cd "$HERE"
python3 tools/snesconv.py build/export .
