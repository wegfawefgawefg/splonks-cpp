#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_cmd cmake
require_cmd git

case "$(uname -s)" in
    Linux)
        require_cmd pkg-config
        ;;
    Darwin)
        if ! xcode-select -p >/dev/null 2>&1; then
            echo "Missing Xcode command line tools. Run: xcode-select --install" >&2
            exit 1
        fi
        ;;
    MINGW*|MSYS*|CYGWIN*)
        if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
            echo "Use the MSYS2 UCRT64 terminal. Current MSYSTEM=${MSYSTEM:-unset}" >&2
            exit 1
        fi
        require_cmd gcc
        require_cmd ninja
        ;;
esac

cd "${repo_root}"

preset="${SPLONKS_PRESET:-dev}"
cmake --list-presets >/dev/null
SPLONKS_PRESET="${preset}" "${repo_root}/scripts/build.sh" --configure-only

echo "[verify] Splonks ${preset} configure path is ready"
