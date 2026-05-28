#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "setup_macos.sh must run on macOS" >&2
    exit 1
fi

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "Splonks supports Apple Silicon macOS development and release packaging only." >&2
    echo "Current architecture: $(uname -m)" >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Missing Xcode command line tools." >&2
    echo "Run: xcode-select --install" >&2
    exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
    echo "Missing Homebrew." >&2
    echo "Install Homebrew from https://brew.sh, then rerun this script." >&2
    exit 1
fi

brew install cmake ninja pkg-config

echo "[setup] macOS Apple Silicon desktop build dependencies installed"
