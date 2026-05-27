#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build"
mode="${1:-build}"
preset="${SPLONKS_PRESET:-release}"
case "${preset}" in
    dev) build_dir="${repo_root}/build-debug" ;;
    package-linux) build_dir="${repo_root}/build-package-linux" ;;
    package-macos) build_dir="${repo_root}/build-package-macos" ;;
    package-windows) build_dir="${repo_root}/build-package-windows" ;;
esac

configure() {
    cmake --preset "${preset}"
}

if ! configure; then
    rm -f "${build_dir}/CMakeCache.txt"
    rm -rf "${build_dir}/CMakeFiles"
    configure
fi

if [ "${mode}" = "--configure-only" ]; then
    exit 0
fi

cmake --build --preset "${preset}" -j
