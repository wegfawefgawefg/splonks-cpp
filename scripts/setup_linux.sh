#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "setup_linux.sh must run on Linux" >&2
    exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This setup script currently supports Debian/Ubuntu through apt-get." >&2
    echo "Install the packages listed in docs/dev_setup.md for other distros." >&2
    exit 1
fi

packages=(
    build-essential
    cmake
    ninja-build
    pkg-config
    wayland-protocols
    libwayland-dev
    libxkbcommon-dev
    libx11-dev
    libxext-dev
    libxcursor-dev
    libxi-dev
    libxfixes-dev
    libxrandr-dev
    libxrender-dev
    libxss-dev
    libasound2-dev
    libpulse-dev
    libpipewire-0.3-dev
    libdecor-0-dev
    libdrm-dev
    libgbm-dev
    libudev-dev
)

sudo apt-get update
sudo apt-get install -y "${packages[@]}"

echo "[setup] Linux desktop build dependencies installed"
