#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build/macos/clap"
DEST="/Library/Audio/Plug-Ins/CLAP"

if [ "${1:-}" = "--user" ]; then
    DEST="$HOME/Library/Audio/Plug-Ins/CLAP"
fi

"$ROOT/build-macos.sh"

SUDO=""
if [ ! -w "$DEST" ] && [ "$DEST" = "/Library/Audio/Plug-Ins/CLAP" ]; then
    SUDO="sudo"
fi

BUNDLE="$BUILD_DIR/Reverie.clap"
$SUDO mkdir -p "$DEST"
$SUDO rm -rf "$DEST/Reverie.clap"
$SUDO cp -R "$BUNDLE" "$DEST/Reverie.clap"
if [ "$DEST" = "/Library/Audio/Plug-Ins/CLAP" ] && [ -e "$HOME/Library/Audio/Plug-Ins/CLAP/Reverie.clap" ]; then
    rm -rf "$HOME/Library/Audio/Plug-Ins/CLAP/Reverie.clap"
fi
echo "Installed $DEST/Reverie.clap"

if pgrep -x "OsxFL" >/dev/null 2>&1; then
    echo "NOTE: FL Studio (OsxFL) is currently running. Please Quit (Cmd+Q) and reopen FL Studio so macOS loads the updated .clap binary."
else
    echo "Done! Open your CLAP host (e.g., FL Studio -> Options -> Manage Plugins -> Find installed plugins)."
fi
