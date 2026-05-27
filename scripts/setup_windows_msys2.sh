#!/usr/bin/env bash
set -euo pipefail

case "${OS:-}:$(uname -s)" in
    Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) ;;
    *)
        echo "setup_windows_msys2.sh must run inside MSYS2 on Windows." >&2
        exit 1
        ;;
esac

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "Open the 'MSYS2 UCRT64' terminal and rerun this script." >&2
    echo "Current MSYSTEM=${MSYSTEM:-unset}" >&2
    exit 1
fi

if ! command -v pacman >/dev/null 2>&1; then
    echo "Missing pacman. Install and run this from MSYS2 UCRT64." >&2
    exit 1
fi

pacman -S --needed --noconfirm \
    git \
    mingw-w64-ucrt-x86_64-cmake \
    mingw-w64-ucrt-x86_64-ninja \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-pkgconf

echo "[setup] Windows MSYS2/UCRT64 build dependencies installed"
