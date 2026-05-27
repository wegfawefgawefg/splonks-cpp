#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
profile_dir="${HOME}/Library/MobileDevice/Provisioning Profiles"
work_dir="${RUNNER_TEMP:-${repo_root}/build/ios-signing}"
keychain_name="${SPLONKS_IOS_KEYCHAIN_NAME:-splonks-ios-signing.keychain-db}"
keychain_path="${RUNNER_TEMP:-${HOME}/Library/Keychains}/${keychain_name}"
keychain_password="${SPLONKS_IOS_KEYCHAIN_PASSWORD:-}"
certificate_path="${work_dir}/ios-signing-certificate.p12"
profile_path="${work_dir}/splonks.mobileprovision"

usage() {
    cat >&2 <<EOF
Usage: $0

Imports iOS distribution signing assets into a temporary keychain and installs
the provisioning profile for non-interactive archive/export builds.

Required environment:
  SPLONKS_IOS_CERTIFICATE_BASE64              Base64-encoded .p12 certificate
  SPLONKS_IOS_CERTIFICATE_PASSWORD            Password for the .p12 certificate
  SPLONKS_IOS_PROVISIONING_PROFILE_BASE64     Base64-encoded .mobileprovision
  SPLONKS_IOS_KEYCHAIN_PASSWORD               Temporary keychain password

Optional environment:
  SPLONKS_IOS_KEYCHAIN_NAME                   Keychain filename
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_env() {
    local name="$1"
    if [[ -z "${!name:-}" ]]; then
        echo "Missing ${name}." >&2
        exit 1
    fi
}

decode_base64() {
    local value="$1"
    local output="$2"
    if base64 --help 2>&1 | grep -q -- '--decode'; then
        printf '%s' "${value}" | base64 --decode > "${output}"
    else
        printf '%s' "${value}" | base64 -D > "${output}"
    fi
}

extract_profile_field() {
    local field="$1"
    security cms -D -i "${profile_path}" | plutil -extract "${field}" raw - 2>/dev/null || true
}

case "${1:-}" in
    -h|--help|help)
        usage
        exit 0
        ;;
    "")
        ;;
    *)
        usage
        exit 1
        ;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "import_signing_assets.sh must run on macOS." >&2
    exit 1
fi

require_cmd base64
require_cmd mkdir
require_cmd security
require_cmd plutil

require_env SPLONKS_IOS_CERTIFICATE_BASE64
require_env SPLONKS_IOS_CERTIFICATE_PASSWORD
require_env SPLONKS_IOS_PROVISIONING_PROFILE_BASE64
require_env SPLONKS_IOS_KEYCHAIN_PASSWORD

mkdir -p "${work_dir}" "${profile_dir}"
chmod 700 "${work_dir}"

decode_base64 "${SPLONKS_IOS_CERTIFICATE_BASE64}" "${certificate_path}"
decode_base64 "${SPLONKS_IOS_PROVISIONING_PROFILE_BASE64}" "${profile_path}"

security delete-keychain "${keychain_path}" >/dev/null 2>&1 || true
security create-keychain -p "${keychain_password}" "${keychain_path}"
security set-keychain-settings -lut 21600 "${keychain_path}"
security unlock-keychain -p "${keychain_password}" "${keychain_path}"
security import "${certificate_path}" \
    -P "${SPLONKS_IOS_CERTIFICATE_PASSWORD}" \
    -A \
    -t cert \
    -f pkcs12 \
    -k "${keychain_path}"
security set-key-partition-list \
    -S apple-tool:,apple:,codesign: \
    -s \
    -k "${keychain_password}" \
    "${keychain_path}"

security list-keychains -d user -s "${keychain_path}" $(security list-keychains -d user | tr -d '"')

profile_uuid="$(extract_profile_field UUID)"
profile_name="$(extract_profile_field Name)"
if [[ -z "${profile_uuid}" ]]; then
    echo "Could not read UUID from provisioning profile." >&2
    exit 1
fi

installed_profile="${profile_dir}/${profile_uuid}.mobileprovision"
cp "${profile_path}" "${installed_profile}"

if [[ -n "${profile_name}" && -z "${SPLONKS_IOS_PROVISIONING_PROFILE:-}" ]]; then
    echo "SPLONKS_IOS_PROVISIONING_PROFILE=${profile_name}" >> "${GITHUB_ENV:-/dev/null}" 2>/dev/null || true
fi

echo "[ios-signing] imported certificate into ${keychain_path}"
echo "[ios-signing] installed provisioning profile ${profile_uuid}"
if [[ -n "${profile_name}" ]]; then
    echo "[ios-signing] profile name=${profile_name}"
fi
