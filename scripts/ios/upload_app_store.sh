#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
release_dir="${repo_root}/dist/releases"
ipa_path="${SPLONKS_IOS_IPA_PATH:-${release_dir}/splonks-${version}-ios.ipa}"
mode="${1:-validate-upload}"

usage() {
    cat >&2 <<EOF
Usage: $0 [validate|upload|validate-upload]

Validates or uploads the signed iOS IPA to App Store Connect/TestFlight.
Run scripts/ios/archive_release.sh first.

Authentication options:
  SPLONKS_APP_STORE_API_KEY       App Store Connect API key ID
  SPLONKS_APP_STORE_API_ISSUER    App Store Connect API issuer ID

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

run_altool() {
    local action="$1"
    local args=(
        "${action}"
        -f "${ipa_path}"
        -t ios
        --output-format xml
    )

    if [[ -n "${SPLONKS_APP_STORE_API_KEY:-}" && -n "${SPLONKS_APP_STORE_API_ISSUER:-}" ]]; then
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
