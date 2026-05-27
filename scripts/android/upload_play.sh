#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

version_name="${SPLONKS_ANDROID_VERSION_NAME:-${SPLONKS_RELEASE_VERSION:-0.1.0}}"
dist_dir="${REPO_ROOT}/dist/splonks-android"
aab_path="${SPLONKS_ANDROID_AAB_PATH:-${dist_dir}/splonks-${version_name}-android-release.aab}"
manifest_path="${dist_dir}/manifest.txt"
package_name="${SPLONKS_ANDROID_PACKAGE_NAME:-${APP_ID}}"
track="${SPLONKS_PLAY_TRACK:-internal}"
release_status="${SPLONKS_PLAY_RELEASE_STATUS:-draft}"
validation_dir="${REPO_ROOT}/dist/validation"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_path="${validation_dir}/android-play-${track}-${timestamp}.log"

usage() {
    cat >&2 <<EOF
Usage: $0 [--validate-only]

Verifies the signed Android release AAB, then uploads it to Google Play through
Fastlane supply. Defaults to the internal track and draft release status.

Environment:
  SPLONKS_PLAY_SERVICE_ACCOUNT_JSON  Google Play service account JSON key file.
  SPLONKS_ANDROID_PACKAGE_NAME       Package name, default: ${package_name}
  SPLONKS_ANDROID_AAB_PATH           AAB path, default: ${aab_path}
  SPLONKS_ANDROID_VERSION_NAME       Version in artifact name, default: ${version_name}
  SPLONKS_PLAY_TRACK                 Play track, default: internal
  SPLONKS_PLAY_RELEASE_STATUS        draft|completed|halted|inProgress, default: draft
  SPLONKS_PLAY_VALIDATE_ONLY         Set to 1 to validate without publishing changes.

Options:
  --validate-only                    Same as SPLONKS_PLAY_VALIDATE_ONLY=1.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --validate-only)
            SPLONKS_PLAY_VALIDATE_ONLY=1
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

if [[ -z "${SPLONKS_PLAY_SERVICE_ACCOUNT_JSON:-}" ]]; then
    echo "Missing SPLONKS_PLAY_SERVICE_ACCOUNT_JSON for Google Play upload." >&2
    exit 1
fi
if [[ ! -f "${SPLONKS_PLAY_SERVICE_ACCOUNT_JSON}" ]]; then
    echo "Google Play service account JSON not found: ${SPLONKS_PLAY_SERVICE_ACCOUNT_JSON}" >&2
    exit 1
fi

require_cmd fastlane

mkdir -p "${validation_dir}"
exec > >(tee "${log_path}") 2>&1

require_upload_key_evidence() {
    local expected_revision
    local release_log
    expected_revision="$(git -C "${REPO_ROOT}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
    while IFS= read -r release_log; do
        if grep -Fxq "git_revision=${expected_revision}" "${release_log}" &&
            grep -Fxq "release_version=${version_name}" "${release_log}" &&
            grep -Fq "[validated] android signed release AAB with upload key" "${release_log}"; then
            echo "[play-upload] upload-key AAB evidence=${release_log}"
            return
        fi
    done < <(find "${validation_dir}" -maxdepth 1 -type f -name "*-android-release-*.log" 2>/dev/null | sort -r)

    echo "Missing Android upload-key AAB validation evidence for version ${version_name}." >&2
    echo "Run with the real upload key first: SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload ./scripts/validate_platform.sh android-release" >&2
    exit 1
}

echo "[play-upload] package_name=${package_name}"
echo "release_version=${version_name}"
echo "git_revision=$(git -C "${REPO_ROOT}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
echo "[play-upload] track=${track}"
echo "[play-upload] release_status=${release_status}"
echo "[play-upload] validate_only=${SPLONKS_PLAY_VALIDATE_ONLY:-0}"
echo "[play-upload] aab=${aab_path}"
echo "[play-upload] manifest=${manifest_path}"

"${REPO_ROOT}/scripts/android/verify_release_aab.sh"
require_upload_key_evidence

cmd=(
    fastlane supply
    --json_key "${SPLONKS_PLAY_SERVICE_ACCOUNT_JSON}"
    --package_name "${package_name}"
    --aab "${aab_path}"
    --track "${track}"
    --release_status "${release_status}"
    --skip_upload_metadata true
    --skip_upload_images true
    --skip_upload_screenshots true
)

if [[ "${SPLONKS_PLAY_VALIDATE_ONLY:-0}" == "1" ]]; then
    cmd+=(--validate_only true)
fi

echo "[play-upload] ${cmd[*]}"
"${cmd[@]}"

if [[ "${SPLONKS_PLAY_VALIDATE_ONLY:-0}" == "1" ]]; then
    echo "[play-upload] validate-only complete for ${aab_path}"
else
    echo "[play-upload] upload complete for ${aab_path}"
fi
echo "[play-upload] wrote ${log_path}"
