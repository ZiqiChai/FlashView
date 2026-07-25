#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# FlashView - build script
#
# Usage:
#   ./scripts/build.sh                 # Release build into ./build
#   ./scripts/build.sh --debug        # Debug build
#   ./scripts/build.sh --clean        # remove build dir first
#   BUILD_DIR=out ./scripts/build.sh  # custom build directory
# ---------------------------------------------------------------------------
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
BUILD_TYPE="Release"
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --debug) BUILD_TYPE="Debug" ;;
        --clean) CLEAN=1 ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [[ $CLEAN -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "==> Cleaning $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

# Prefer Ninja when available (faster incremental builds)
GENERATOR=()
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=(-G Ninja)
fi

echo "==> Configuring ($BUILD_TYPE)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" "${GENERATOR[@]}" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

echo ""
echo "==> Done: $BUILD_DIR/flashview"
