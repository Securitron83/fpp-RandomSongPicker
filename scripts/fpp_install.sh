#!/bin/bash
# fpp-RandomSongPicker install script.
# Called by FPP when the plugin is installed or updated.

set -e

PLUGIN_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ -d /opt/fpp/src ]; then
    FPP_SRC=/opt/fpp/src
elif [ -n "$FPPDIR" ] && [ -d "$FPPDIR/src" ]; then
    FPP_SRC="$FPPDIR/src"
else
    echo "ERROR: Cannot find FPP source directory. Set FPPDIR or install FPP to /opt/fpp."
    exit 1
fi

echo "fpp-RandomSongPicker: Installing build dependencies..."
apt-get install -y --no-install-recommends \
    g++ \
    make \
    libjsoncpp-dev \
    libcurl4-openssl-dev

echo "fpp-RandomSongPicker: Building C++ plugin (FPP_SRC=${FPP_SRC})..."
cd "${PLUGIN_DIR}"
make FPP_SRC="${FPP_SRC}" clean
make FPP_SRC="${FPP_SRC}" -j"$(nproc)"

echo "fpp-RandomSongPicker: Install complete."
echo "  Restart fppd for the new command to appear."
echo "  Add 'Playlist - Pick Random Song' as a Command entry in your playlist."
