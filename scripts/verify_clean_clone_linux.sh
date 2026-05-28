#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "verify_clean_clone_linux.sh must run on Linux" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
revision="$(git -C "${repo_root}" rev-parse HEAD)"
work_root="${SPLONKS_CLEAN_CLONE_ROOT:-}"
keep_clone="${SPLONKS_KEEP_CLEAN_CLONE:-0}"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

usage() {
    cat >&2 <<EOF
Usage: $0

Clones the current Splonks checkout into a temporary directory, checks out the
current commit, and runs the Linux dev onboarding verification path from that
clean clone. This proves the Linux developer path does not rely on adjacent
repos or generated files in the working checkout.

Environment:
  SPLONKS_CLEAN_CLONE_ROOT   Optional directory to use instead of mktemp.
  SPLONKS_KEEP_CLEAN_CLONE   Set to 1 to keep the temporary clone after the run.
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

require_cmd git

if [[ -z "${work_root}" ]]; then
    work_root="$(mktemp -d)"
else
    rm -rf "${work_root}"
    mkdir -p "${work_root}"
fi

cleanup() {
    if [[ "${keep_clone}" != "1" ]]; then
        rm -rf "${work_root}"
    else
        echo "[clean-clone] kept ${work_root}"
    fi
}
trap cleanup EXIT

clone_dir="${work_root}/splonks-cpp"
git clone --no-local "${repo_root}" "${clone_dir}"
git -C "${clone_dir}" checkout "${revision}"

(
    cd "${clone_dir}"
    ./scripts/bootstrap_dev.sh --skip-setup
)

echo "[clean-clone] Linux dev onboarding verified at ${revision:0:12}"
