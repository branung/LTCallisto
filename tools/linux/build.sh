#!/bin/bash
# Builds LTCallisto for Linux with the Direct3D 9 renderer over dxvk-native. Run tools/linux/setup.sh first.
#
#   tools/linux/build.sh [cmake options...]
#
# From Windows: wsl -d Ubuntu-22.04 -u root -- bash /mnt/c/Projects/lithtech/tools/linux/build.sh
set -euo pipefail

REPO=$(cd "$(dirname "$0")/../.." && pwd)
SRC=${LT_LINUX_SRC:-$HOME/lithtech-src}
BUILD=${LT_LINUX_BUILD:-$HOME/lithtech-build}
export PKG_CONFIG_PATH=/opt/dxvk-native/usr/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}

# The stamp of the Windows build in the same checkout, so the two builds share a version.
# LTCALLISTO_STAMP set by the caller wins, and with neither the build stamps itself with the time
WIN_STAMP=${LT_WINDOWS_BUILD:-$REPO/build}/tools/launcher/ltcallisto_stamp.txt
if [ -z "${LTCALLISTO_STAMP:-}" ] && [ -f "$WIN_STAMP" ]; then
    LTCALLISTO_STAMP=$(tr -d '\r\n' < "$WIN_STAMP")
fi
export LTCALLISTO_STAMP=${LTCALLISTO_STAMP:-}
echo "stamp: ${LTCALLISTO_STAMP:-the time of this build}"

mkdir -p "$SRC" "$BUILD"
rsync -a --delete --exclude /build/ --exclude /.git/ --exclude __pycache__/ "$REPO/" "$SRC/"
echo "sources copied from $REPO to $SRC"

if [ ! -f "$BUILD/CMakeCache.txt" ] || [ $# -gt 0 ]; then
    cmake -S "$SRC" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DENABLE_DXVK=ON \
        -DBUILD_SHOGO=ON -DBUILD_SHOGO_OBJECTDLL=ON -DBUILD_NOLF2=ON -DBUILD_BLOOD2=ON \
        -DBUILD_LAUNCHER=ON "$@" >"$BUILD/configure.log" 2>&1 \
        || { tail -30 "$BUILD/configure.log"; echo "configure failed, see $BUILD/configure.log"; exit 1; }
    echo "configured"
fi

set +e
make -C "$BUILD" -j"$(nproc)" ${LT_MAKE_FLAGS:-} >"$BUILD/build.log" 2>&1
CODE=$?
set -e
if [ $CODE -ne 0 ]; then
    grep -E "error|Error [0-9]" "$BUILD/build.log" | head -40 || true
    echo "build failed ($CODE), see $BUILD/build.log"
    exit $CODE
fi
echo "built: $BUILD/OUT"
