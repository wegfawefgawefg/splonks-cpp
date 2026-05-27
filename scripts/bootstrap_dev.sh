#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
configure_only=0
skip_setup=0
launch_game=0

usage() {
    cat >&2 <<EOF
Usage: $0 [--configure-only] [--skip-setup] [--run]

Installs/checks the supported desktop build dependencies for this host, then
configures, builds, and runs the Splonks dev smoke test.

Options:
  --configure-only   Stop after CMake configure.
  --skip-setup       Do not run the platform package-manager setup script.
  --run              Launch the game after the dev smoke passes.

Environment:
  SPLONKS_PRESET     CMake preset to verify, default: dev
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --configure-only)
            configure_only=1
            ;;
        --skip-setup)
            skip_setup=1
            ;;
        --run)
            launch_game=1
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            usage
            exit 1
            ;;
    esac
    shift
done

host_platform() {
    case "${OS:-}:$(uname -s)" in
        Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) printf 'windows' ;;
        *:Darwin) printf 'macos' ;;
        *:Linux) printf 'linux' ;;
        *)
            echo "Unsupported host platform: ${OS:-}:$(uname -s)" >&2
            exit 1
            ;;
    esac
}

platform="$(host_platform)"

if [[ "${configure_only}" -eq 1 && "${launch_game}" -eq 1 ]]; then
    echo "--configure-only and --run cannot be used together." >&2
    exit 1
fi

if [[ "${skip_setup}" -eq 0 ]]; then
    case "${platform}" in
        linux)
            "${repo_root}/scripts/setup_linux.sh"
            ;;
        macos)
            "${repo_root}/scripts/setup_macos.sh"
            ;;
        windows)
            "${repo_root}/scripts/setup_windows_msys2.sh"
            ;;
    esac
else
    echo "[bootstrap] skipping platform setup"
fi

verify_args=()
if [[ "${configure_only}" -eq 1 ]]; then
    verify_args+=(--configure-only)
fi

env SPLONKS_PRESET="${SPLONKS_PRESET:-dev}" \
    "${repo_root}/scripts/verify_dev_env.sh" "${verify_args[@]}"

echo "[bootstrap] ${platform} developer path is ready"
if [[ "${launch_game}" -eq 1 ]]; then
    echo "[bootstrap] launching Splonks"
    env SPLONKS_PRESET="${SPLONKS_PRESET:-dev}" "${repo_root}/scripts/run.sh"
else
    echo "[bootstrap] launch with: SPLONKS_PRESET=${SPLONKS_PRESET:-dev} ./scripts/run.sh"
fi
