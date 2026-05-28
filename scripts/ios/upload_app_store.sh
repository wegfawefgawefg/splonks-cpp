#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"
ipa_path="${SPLONKS_IOS_IPA_PATH:-${release_dir}/splonks-${version}-ios.ipa}"
mode="${1:-validate-upload}"
validation_dir="${repo_root}/dist/validation"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
log_path="${validation_dir}/ios-upload-${mode}-${timestamp}.log"

usage() {
    cat >&2 <<EOF
Usage: $0 [validate|upload|validate-upload]

Validates or uploads the signed iOS IPA to App Store Connect/TestFlight.
Run scripts/ios/archive_release.sh first.

Authentication options:
  SPLONKS_APP_STORE_API_KEY       App Store Connect API key ID
  SPLONKS_APP_STORE_API_ISSUER    App Store Connect API issuer ID
  API_PRIVATE_KEYS_DIR            Directory containing AuthKey_<key id>.p8
  SPLONKS_APP_STORE_API_PRIVATE_KEYS_DIR
                                  Optional project-specific alias for API_PRIVATE_KEYS_DIR

or:
  APPLE_ID                        Apple account email
  APPLE_APP_SPECIFIC_PASSWORD     App-specific password

Environment:
  SPLONKS_RELEASE_VERSION         Version in the default IPA path, default: ${version}
  SPLONKS_IOS_IPA_PATH            Optional explicit IPA path
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

configure_api_private_key_dir() {
    if [[ -n "${SPLONKS_APP_STORE_API_PRIVATE_KEYS_DIR:-}" ]]; then
        export API_PRIVATE_KEYS_DIR="${SPLONKS_APP_STORE_API_PRIVATE_KEYS_DIR}"
    fi
}

require_api_private_key() {
    local key_id="$1"
    local filename="AuthKey_${key_id}.p8"
    local search_dirs=()
    if [[ -n "${API_PRIVATE_KEYS_DIR:-}" ]]; then
        search_dirs+=("${API_PRIVATE_KEYS_DIR}")
    fi
    search_dirs+=(
        "./private_keys"
        "${HOME}/private_keys"
        "${HOME}/.private_keys"
        "${HOME}/.appstoreconnect/private_keys"
    )

    local dir
    for dir in "${search_dirs[@]}"; do
        if [[ -f "${dir}/${filename}" ]]; then
            return 0
        fi
    done

    echo "Missing App Store Connect API private key: ${filename}" >&2
    echo "Place it in one of altool's private key directories or set API_PRIVATE_KEYS_DIR." >&2
    printf 'Searched:\n' >&2
    for dir in "${search_dirs[@]}"; do
        printf '  %s\n' "${dir}" >&2
    done
    exit 1
}

run_altool() {
    local action="$1"
    local args=(
        "${action}"
        -f "${ipa_path}"
        -t ios
        --output-format xml
    )

    if [[ -n "${SPLONKS_APP_STORE_API_KEY:-}" && -n "${SPLONKS_APP_STORE_API_ISSUER:-}" ]]; then
        configure_api_private_key_dir
        require_api_private_key "${SPLONKS_APP_STORE_API_KEY}"
        args+=(
            --apiKey "${SPLONKS_APP_STORE_API_KEY}"
            --apiIssuer "${SPLONKS_APP_STORE_API_ISSUER}"
        )
    elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_APP_SPECIFIC_PASSWORD:-}" ]]; then
        args+=(
            -u "${APPLE_ID}"
            -p "${APPLE_APP_SPECIFIC_PASSWORD}"
        )
    else
        echo "Missing App Store Connect credentials." >&2
        echo "Set API key env vars or APPLE_ID plus APPLE_APP_SPECIFIC_PASSWORD." >&2
        exit 1
    fi

    echo "[ios-upload] xcrun altool ${action} -f ${ipa_path} -t ios"
    xcrun altool "${args[@]}"
}

case "${mode}" in
    validate|upload|validate-upload) ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "upload_app_store.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

require_cmd xcrun

if [[ ! -f "${ipa_path}" ]]; then
    echo "Missing IPA: ${ipa_path}" >&2
    echo "Run scripts/ios/archive_release.sh first or set SPLONKS_IOS_IPA_PATH." >&2
    exit 1
fi

mkdir -p "${validation_dir}"
exec > >(tee "${log_path}") 2>&1

echo "[ios-upload] mode=${mode}"
echo "release_version=${version}"
echo "git_revision=$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
echo "[ios-upload] ipa=${ipa_path}"

"${repo_root}/scripts/ios/verify_release_ipa.sh"

case "${mode}" in
    validate)
        run_altool --validate-app
        ;;
    upload)
        run_altool --upload-app
        ;;
    validate-upload)
        run_altool --validate-app
        run_altool --upload-app
        ;;
esac

echo "[ios-upload] ${mode} complete for ${ipa_path}"
echo "[ios-upload] wrote ${log_path}"
