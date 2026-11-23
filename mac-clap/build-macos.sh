#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$ROOT/build/macos"
GEN_ARGS=()
if command -v ninja >/dev/null 2>&1; then
    GEN_ARGS=(-G Ninja)
fi

cmake -S "$ROOT" -B "$BUILD_DIR" "${GEN_ARGS[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

BUNDLE="$BUILD_DIR/clap/Reverie.clap"
echo "Ad-hoc signing $BUNDLE..."
codesign --force --deep --sign - "$BUNDLE"

echo "Running CLAP host smoke tests..."
"$BUILD_DIR/host_test" "$BUNDLE"
