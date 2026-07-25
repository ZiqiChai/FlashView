#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# FlashView - build & install
#
# Usage:
#   ./install.sh                     # build (Release) and install to /usr/local
#   ./install.sh --prefix ~/.local  # install somewhere else (no sudo needed)
#   ./install.sh --deps             # also install build dependencies first
#   ./install.sh --uninstall        # remove installed files
# ---------------------------------------------------------------------------
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
INSTALL_PREFIX="/usr/local"
INSTALL_DEPS=0
UNINSTALL=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)
            INSTALL_PREFIX="$(realpath -m "$2")"
            shift 2
            ;;
        --deps)
            INSTALL_DEPS=1
            shift
            ;;
        --uninstall)
            UNINSTALL=1
            shift
            ;;
        -h|--help)
            grep '^#' "$0" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
done

# Use sudo only when the prefix is not writable by the current user.
SUDO=""
if [[ ! -w "$INSTALL_PREFIX" && $EUID -ne 0 ]]; then
    SUDO="sudo"
fi

if [[ $UNINSTALL -eq 1 ]]; then
    echo "=== Uninstalling FlashView from $INSTALL_PREFIX ==="
    $SUDO rm -f "$INSTALL_PREFIX/bin/flashview"
    $SUDO rm -f "$INSTALL_PREFIX/share/applications/flashview.desktop"
    $SUDO rm -f "$INSTALL_PREFIX/share/icons/hicolor/scalable/apps/flashview.svg"
    $SUDO update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
    echo "Done."
    exit 0
fi

echo "=== FlashView Installer ==="
echo ""

if [[ $INSTALL_DEPS -eq 1 ]]; then
    echo "[0/3] Installing dependencies..."
    "$SCRIPT_DIR/scripts/install-deps.sh"
fi

echo "[1/3] Building (Release)..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX"
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
echo "  Build complete."

echo "[2/3] Installing to $INSTALL_PREFIX ..."
$SUDO cmake --install "$BUILD_DIR"
echo "  Installed."

echo "[3/3] Updating desktop database..."
$SUDO update-desktop-database "$INSTALL_PREFIX/share/applications" 2>/dev/null || true
$SUDO gtk-update-icon-cache "$INSTALL_PREFIX/share/icons/hicolor/" 2>/dev/null || true
echo "  Done."

echo ""
echo "FlashView has been installed!"
echo "  - Run: flashview [file_or_directory]"
echo "  - Or find it in your application menu"
echo ""
echo "To uninstall:  ./install.sh --uninstall --prefix $INSTALL_PREFIX"
