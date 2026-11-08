#!/bin/bash
# Prepares a Linux box (Ubuntu 22.04 WSL distribution used for reference) to build LTCallisto
# with the Direct3D 9 renderer over dxvk-native. Run as root. Safe to run again:
# each step is skipped when it's already done.
#
#   tools/linux/setup.sh
#
# From Windows: wsl -d Ubuntu-22.04 -u root -- bash /mnt/c/Projects/lithtech/tools/linux/setup.sh
set -euo pipefail

DXVK_VERSION=3.1.1
PREFIX=/opt/dxvk-native

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq \
    build-essential cmake pkg-config git curl python3 rsync \
    libsdl2-dev libopenal-dev libmpg123-dev libvulkan-dev libfreetype-dev \
    mesa-vulkan-drivers vulkan-tools xvfb >/dev/null
echo "packages installed"

# dxvk-native isn't packaged by Ubuntu. DXVK ships a native build with each release.
# Its .pc files carry the release machine's prefix, so they're pointed at PREFIX
PC=$PREFIX/usr/lib/pkgconfig/dxvk-d3d9.pc
if [ -f "$PC" ] && grep -q "^Version: $DXVK_VERSION\$" "$PC"; then
    echo "dxvk-native $DXVK_VERSION already in $PREFIX"
else
    TMP=$(mktemp -d)
    curl -fsSL -o "$TMP/dxvk.tar.gz" \
        "https://github.com/doitsujin/dxvk/releases/download/v$DXVK_VERSION/dxvk-native-$DXVK_VERSION-steamrt-sniper.tar.gz"
    mkdir -p "$PREFIX"
    tar xzf "$TMP/dxvk.tar.gz" -C "$PREFIX"
    rm -rf "$TMP"
    echo "dxvk-native $DXVK_VERSION installed in $PREFIX"
fi
sed -i "s%^prefix=.*%prefix=$PREFIX/usr%" "$PREFIX"/usr/lib/pkgconfig/*.pc
PKG_CONFIG_PATH=$PREFIX/usr/lib/pkgconfig pkg-config --modversion dxvk-d3d9
