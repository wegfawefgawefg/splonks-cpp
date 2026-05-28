#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
skip_launch="${SPLONKS_SKIP_INTERACTIVE_LAUNCH:-0}"

usage() {
    cat >&2 <<EOF
Usage: $0

Runs the full desktop developer/release handoff path for the current host:
setup/build/smoke, optional interactive launch, dev validation, release
package validation, and evidence bundling.

Environment:
  SPLONKS_RELEASE_VERSION           Release version, default: ${version}
  SPLONKS_SKIP_INTERACTIVE_LAUNCH   Set to 1 to skip launching the game window.
EOF
}

case "${1:-}" in
    "")
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

if [[ "${platform}" == "windows" && "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "Windows handoff validation must run in the MSYS2 UCRT64 terminal. Current MSYSTEM=${MSYSTEM:-unset}" >&2
    exit 1
fi

if [[ "${platform}" == "macos" && "$(uname -m)" != "arm64" ]]; then
    echo "macOS release validation requires an Apple Silicon arm64 Mac." >&2
    exit 1
fi

cd "${repo_root}"

echo "[handoff] platform=${platform}"
echo "[handoff] release_version=${version}"
echo "[handoff] revision=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"

if [[ "${skip_launch}" == "1" ]]; then
    ./scripts/bootstrap_dev.sh
    echo "[handoff] skipped interactive launch"
else
    ./scripts/bootstrap_dev.sh --run
fi

SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh dev
SPLONKS_RELEASE_VERSION="${version}" ./scripts/validate_platform.sh release
./scripts/bundle_validation_evidence.sh --include-artifacts "${platform}"

echo "[handoff] ${platform} desktop handoff complete"
