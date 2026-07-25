#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# FlashView - dependency installer for Debian/Ubuntu (22.04 / 24.04 / 26.04)
#
# Installs the toolchain and Qt6 development packages required to build
# FlashView. Safe to re-run; apt skips packages that are already installed.
#
# Usage:
#   ./scripts/install-deps.sh          # interactive (asks for sudo)
#   ./scripts/install-deps.sh -y       # non-interactive (CI)
# ---------------------------------------------------------------------------
set -euo pipefail

APT_FLAGS=()
if [[ "${1:-}" == "-y" || -n "${CI:-}" ]]; then
    APT_FLAGS+=(-y)
    export DEBIAN_FRONTEND=noninteractive
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "error: this script supports Debian/Ubuntu (apt-get) only." >&2
    echo "Please install the following manually:" >&2
    echo "  - CMake >= 3.16, a C++17 compiler" >&2
    echo "  - Qt6: Core, Gui, Widgets, Concurrent, Test, Multimedia, MultimediaWidgets, LinguistTools" >&2
    exit 1
fi

SUDO=""
if [[ $EUID -ne 0 ]]; then
    SUDO="sudo"
fi

# Toolchain
PACKAGES=(
    build-essential
    cmake
    ninja-build
)

# Qt6 development packages
#   qt6-base-dev            -> Qt6::Core / Gui / Widgets / Concurrent / Test
#   qt6-multimedia-dev      -> Qt6::Multimedia / MultimediaWidgets
#   qt6-tools-dev           -> Qt6LinguistTools CMake config
#   qt6-tools-dev-tools     -> Qt6 tool binaries
#   qt6-l10n-tools          -> lrelease/lconvert used by qt_add_translations
#   libgl1-mesa-dev         -> OpenGL headers required when linking Qt6::Gui
PACKAGES+=(
    qt6-base-dev
    qt6-multimedia-dev
    qt6-tools-dev
    qt6-tools-dev-tools
    qt6-l10n-tools
    libgl1-mesa-dev
)

# Runtime multimedia backend (optional for building, needed to play videos)
PACKAGES+=(
    libqt6multimedia6
    libqt6svg6
    qt6-image-formats-plugins
    gstreamer1.0-plugins-base
    gstreamer1.0-plugins-good
)

echo "==> Installing: ${PACKAGES[*]}"
$SUDO apt-get update
$SUDO apt-get install "${APT_FLAGS[@]}" "${PACKAGES[@]}"

echo "==> Done. You can now run ./scripts/build.sh"
