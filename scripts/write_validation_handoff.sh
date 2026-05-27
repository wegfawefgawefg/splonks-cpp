#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
target="${1:-all}"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
revision="$(git -C "${repo_root}" rev-parse HEAD 2>/dev/null || echo unknown)"
short_revision="${revision:0:12}"
out_dir="${SPLONKS_HANDOFF_DIR:-${repo_root}/dist/validation-handoffs}"

usage() {
    cat >&2 <<EOF
Usage: $0 [macos|windows|android-play|ios|all]

Writes a timestamped Markdown handoff packet for external validators. The
packet pins the release version, remote ref, and exact commit revision, then
embeds the copy/paste commands from print_validation_handoff.sh.

Environment:
  SPLONKS_RELEASE_VERSION   Release version used in artifact names, default: ${version}
  SPLONKS_VALIDATION_REF    Remote ref to fetch before checkout.
  SPLONKS_VALIDATION_REVISION
                            Commit to validate, default: current HEAD.
  SPLONKS_HANDOFF_DIR       Output directory, default: dist/validation-handoffs
EOF
}

case "${target}" in
    macos|windows|android-play|ios|all)
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

safe_target="$(printf '%s' "${target}" | tr -c 'A-Za-z0-9._-' '-')"
mkdir -p "${out_dir}"
out_path="${out_dir}/splonks-validation-handoff-${safe_target}-${short_revision}-${timestamp}.md"

{
    echo "# Splonks Validation Handoff"
    echo
    echo "- target: ${target}"
    echo "- release_version: ${version}"
    echo "- validation_revision: ${SPLONKS_VALIDATION_REVISION:-${revision}}"
    echo "- generated_utc: ${timestamp}"
    echo
    echo "Import returned evidence with the receiver commands printed in each target section."
    echo
    env \
        SPLONKS_RELEASE_VERSION="${version}" \
        SPLONKS_VALIDATION_REVISION="${SPLONKS_VALIDATION_REVISION:-${revision}}" \
        "${repo_root}/scripts/print_validation_handoff.sh" "${target}"
} > "${out_path}"

echo "[handoff] ${out_path}"
