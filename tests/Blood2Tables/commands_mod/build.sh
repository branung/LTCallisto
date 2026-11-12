#!/bin/bash
# Builds the commands mod into a rez, with retail 01_SubwayA patched by map.json (a Script and a
# Command object added). The patched world is retail content, so the rez is built from an install
# and never committed. Packed with LithRez's "l" option, without which it leaves out the lower
# case Mod\ directory.
#
#   build.sh <out.rez> [--build DIR]
#
# Needs two things this tree does not carry:
#   B2_REZ        retail BLOOD2.REZ
#   PATCH_LEVELS  a level patcher taking <map.json> <archive.rez:MEMBER> <out.dat>
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
OUTREZ=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
BUILD=${3:-$ROOT/build}
: "${B2_REZ:?set B2_REZ to retail BLOOD2.REZ}"
: "${PATCH_LEVELS:?set PATCH_LEVELS to the level patcher}"
LITHREZ=""
for p in "$BUILD/OUT/LithRez" "$BUILD/OUT/Release/LithRez.exe" "$BUILD/OUT/Release/LithRez"; do
    [ -x "$p" ] && { LITHREZ=$p; break; }
done
[ -n "$LITHREZ" ] || { echo "no LithRez in $BUILD (BUILD_TOOLS)" >&2; exit 1; }
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/tree/WORLDS"
cp -r "$HERE/Mod" "$HERE/Scripts" "$TMP/tree/"
PY=${PYTHON:-python3}
"$PY" -c "" 2>/dev/null || PY=python		# Windows' python3 can be the store stub
"$PY" "$PATCH_LEVELS" "$HERE/map.json" "$B2_REZ:WORLDS/01_SUBWAYA.DAT" "$TMP/tree/WORLDS/01_SubwayA.dat"
rm -f "$OUTREZ"
( cd "$TMP" && "$LITHREZ" cl "$OUTREZ" tree "*.*" >/dev/null 2>&1 )
[ -s "$OUTREZ" ] && echo "$OUTREZ"
