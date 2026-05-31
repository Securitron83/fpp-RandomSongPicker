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
    libjsoncpp-dev

echo "fpp-RandomSongPicker: Building C++ plugin (FPP_SRC=${FPP_SRC})..."
cd "${PLUGIN_DIR}"
make FPP_SRC="${FPP_SRC}" clean
make FPP_SRC="${FPP_SRC}" -j"$(nproc)"

echo "fpp-RandomSongPicker: Install complete."

# Restart fppd so the new command is picked up immediately
if systemctl is-active --quiet fppd 2>/dev/null; then
    echo "fpp-RandomSongPicker: Restarting fppd..."
    systemctl restart fppd
    echo "fpp-RandomSongPicker: fppd restarted — 'Insert Random Item with History' command is now available."
else
    echo "fpp-RandomSongPicker: fppd is not running — start it to load the 'Insert Random Item with History' command."
fi
