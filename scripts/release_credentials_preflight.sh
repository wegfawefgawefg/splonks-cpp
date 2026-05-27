#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
target="${1:-all}"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"

usage() {
    cat >&2 <<EOF
Usage: $0 [macos-notarized|android-release|android-play|ios-release|ios-device|ios-upload|all]

Checks release credential/tool prerequisites without building, notarizing, or
uploading. This is a preflight for external validators before they run the full
validation commands.

Environment:
  SPLONKS_RELEASE_VERSION   Release version in artifact names, default: ${version}
EOF
}

failures=0

ok() {
    printf '[ok]      %s\n' "$1"
}

missing() {
    printf '[missing] %s\n' "$1"
    failures=$((failures + 1))
}

require_cmd() {
    local cmd="$1"
    if command -v "${cmd}" >/dev/null 2>&1; then
        ok "command ${cmd}"
    else
        missing "command ${cmd}"
    fi
}

require_file() {
    local label="$1"
    local path="$2"
    if [[ -n "${path}" && -f "${path}" ]]; then
        ok "${label}: ${path}"
    else
        missing "${label}: ${path:-<unset>}"
    fi
}

require_env() {
    local name="$1"
    if [[ -n "${!name:-}" ]]; then
        ok "${name} is set"
    else
        missing "${name} is not set"
    fi
}

require_manifest_value() {
    local label="$1"
    local path="$2"
    local key="$3"
    local expected="$4"
    local actual
    if [[ ! -f "${path}" ]]; then
        missing "${label}: ${path}"
        return
    fi
    actual="$(awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${path}" | tail -n 1)"
    if [[ "${actual}" == "${expected}" ]]; then
        ok "${label}: ${key}=${expected}"
    else
        missing "${label}: ${key}=${actual:-<unset>}, expected ${expected}"
    fi
}

require_checksum_file() {
    local label="$1"
    local artifact="$2"
    local checksum="$3"
    local expected
    local actual
    require_file "${label}" "${artifact}"
    require_file "${label} checksum" "${checksum}"
    if [[ ! -f "${artifact}" || ! -f "${checksum}" ]]; then
        return
    fi
    expected="$(awk 'NF >= 1 {print tolower($1); exit}' "${checksum}")"
    if command -v sha256sum >/dev/null 2>&1; then
        actual="$(sha256sum "${artifact}" | awk '{print tolower($1)}')"
    elif command -v shasum >/dev/null 2>&1; then
        actual="$(shasum -a 256 "${artifact}" | awk '{print tolower($1)}')"
    else
        missing "command sha256sum or shasum"
        return
    fi
    if [[ -n "${expected}" && "${actual}" == "${expected}" ]]; then
        ok "${label} checksum matches"
    else
        missing "${label} checksum mismatch: ${actual:-<unset>}, expected ${expected:-<unset>}"
    fi
}

require_macos_host() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        ok "macOS host"
    else
        missing "macOS host required; current host is $(uname -s)"
    fi
}

require_macos_arm64_host() {
    require_macos_host
    if [[ "$(uname -s)" == "Darwin" && "$(uname -m)" == "arm64" ]]; then
        ok "Apple Silicon arm64 host"
    else
        missing "Apple Silicon arm64 host required for arm64-only macOS release validation; current arch is $(uname -m)"
    fi
}

check_macos_notarized() {
    echo
    echo "[macos-notarized]"
    require_macos_arm64_host
    require_cmd xcrun
    require_cmd codesign
    require_cmd ditto
    require_cmd spctl
    require_cmd xattr
    require_env SPLONKS_MACOS_SIGN_IDENTITY
    if [[ -n "${SPLONKS_NOTARYTOOL_PROFILE:-}" ]]; then
        ok "SPLONKS_NOTARYTOOL_PROFILE is set"
    else
        require_env APPLE_ID
        require_env APPLE_TEAM_ID
        require_env APPLE_APP_SPECIFIC_PASSWORD
    fi
}

check_android_release() {
    echo
    echo "[android-release]"
    local android_sdk_root="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
    local android_ndk_home="${ANDROID_NDK_HOME:-${android_sdk_root}/ndk/26.3.11579264}"
    local android_dir="${repo_root}/android"
    require_cmd java
    require_cmd keytool
    require_cmd unzip
    require_file "SPLONKS_ANDROID_KEYSTORE" "${SPLONKS_ANDROID_KEYSTORE:-}"
    require_env SPLONKS_ANDROID_KEYSTORE_PASSWORD
    echo "[info] keystore_type=${SPLONKS_ANDROID_KEYSTORE_TYPE:-jks}"
    if [[ "${SPLONKS_ANDROID_KEYSTORE_PURPOSE:-}" == "upload" ]]; then
        ok "SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload"
    else
        missing "SPLONKS_ANDROID_KEYSTORE_PURPOSE must be set to upload for Play-ready release evidence"
    fi
    require_env SPLONKS_ANDROID_KEY_ALIAS
    require_env SPLONKS_ANDROID_KEY_PASSWORD
    if [[ -f "${android_ndk_home}/build/cmake/android.toolchain.cmake" ]]; then
        ok "Android NDK toolchain: ${android_ndk_home}/build/cmake/android.toolchain.cmake"
    else
        missing "Android NDK toolchain: ${android_ndk_home}/build/cmake/android.toolchain.cmake"
    fi
    if compgen -G "${android_dir}/app/libs/SDL3*.aar" >/dev/null; then
        ok "SDL3 Android AAR present"
    else
        missing "SDL3 Android AAR missing; run scripts/android/fetch_sdl3_aar.sh"
    fi
}

check_android_play() {
    echo
    echo "[android-play]"
    local app_id="${APP_ID:-dev.splonks.game}"
    local version_name="${SPLONKS_ANDROID_VERSION_NAME:-${SPLONKS_RELEASE_VERSION:-${version}}}"
    local dist_dir="${repo_root}/dist/splonks-android"
    local aab_path="${SPLONKS_ANDROID_AAB_PATH:-${dist_dir}/splonks-${version_name}-android-release.aab}"
    local manifest_path="${dist_dir}/manifest.txt"
    require_cmd fastlane
    require_file "SPLONKS_PLAY_SERVICE_ACCOUNT_JSON" "${SPLONKS_PLAY_SERVICE_ACCOUNT_JSON:-}"
    require_manifest_value "Android release manifest" "${manifest_path}" version_name "${version_name}"
    if env \
        SPLONKS_ANDROID_VERSION_NAME="${version_name}" \
        SPLONKS_ANDROID_AAB_PATH="${aab_path}" \
        "${repo_root}/scripts/android/verify_release_aab.sh"; then
        ok "Android release AAB verified for Play upload"
    else
        missing "Android release AAB failed verification for Play upload"
    fi
    echo "[info] package=${SPLONKS_ANDROID_PACKAGE_NAME:-${app_id}} track=${SPLONKS_PLAY_TRACK:-internal} status=${SPLONKS_PLAY_RELEASE_STATUS:-draft}"
}

check_ios_release() {
    echo
    echo "[ios-release]"
    require_macos_host
    require_cmd cmake
    require_cmd xcodebuild
    require_env SPLONKS_IOS_DEVELOPMENT_TEAM
    if [[ "${GITHUB_ACTIONS:-}" == "true" ||
        -n "${SPLONKS_IOS_CERTIFICATE_BASE64:-}" ||
        -n "${SPLONKS_IOS_PROVISIONING_PROFILE_BASE64:-}" ]]; then
        require_cmd security
        require_cmd plutil
        require_env SPLONKS_IOS_CERTIFICATE_BASE64
        require_env SPLONKS_IOS_CERTIFICATE_PASSWORD
        require_env SPLONKS_IOS_PROVISIONING_PROFILE_BASE64
        require_env SPLONKS_IOS_KEYCHAIN_PASSWORD
    fi
    echo "[info] bundle=${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game} identity=${SPLONKS_IOS_CODE_SIGN_IDENTITY:-Apple Distribution} export=${SPLONKS_IOS_EXPORT_METHOD:-app-store-connect}"
}

check_ios_device() {
    echo
    echo "[ios-device]"
    local archive_path="${SPLONKS_IOS_ARCHIVE_PATH:-${repo_root}/dist/splonks-ios/Splonks.xcarchive}"
    local app_path="${SPLONKS_IOS_DEVICE_APP_PATH:-${archive_path}/Products/Applications/Splonks.app}"
    require_macos_host
    require_cmd xcrun
    require_env SPLONKS_IOS_DEVICE_ID
    if [[ -d "${app_path}" ]]; then
        ok "iOS device app: ${app_path}"
    else
        missing "iOS device app: ${app_path}"
    fi
    echo "[info] bundle=${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"
}

check_ios_upload() {
    echo
    echo "[ios-upload]"
    local ipa_path="${SPLONKS_IOS_IPA_PATH:-${repo_root}/dist/releases/splonks-${version}-ios.ipa}"
    local manifest_path="${repo_root}/dist/splonks-ios/manifest.txt"
    require_macos_host
    require_cmd xcrun
    require_checksum_file "iOS IPA" "${ipa_path}" "${ipa_path}.sha256"
    require_manifest_value "iOS release manifest" "${manifest_path}" version_name "${version}"
    if [[ -n "${SPLONKS_APP_STORE_API_KEY:-}" && -n "${SPLONKS_APP_STORE_API_ISSUER:-}" ]]; then
        ok "App Store Connect API key/issuer are set"
        local key_dir="${SPLONKS_APP_STORE_API_PRIVATE_KEYS_DIR:-${API_PRIVATE_KEYS_DIR:-}}"
        if [[ -n "${key_dir}" ]]; then
            require_file "AuthKey_${SPLONKS_APP_STORE_API_KEY}.p8" "${key_dir}/AuthKey_${SPLONKS_APP_STORE_API_KEY}.p8"
        else
            missing "API_PRIVATE_KEYS_DIR or SPLONKS_APP_STORE_API_PRIVATE_KEYS_DIR is not set"
        fi
    else
        require_env APPLE_ID
        require_env APPLE_APP_SPECIFIC_PASSWORD
    fi
}

case "${target}" in
    macos-notarized)
        check_macos_notarized
        ;;
    android-release)
        check_android_release
        ;;
    android-play)
        check_android_play
        ;;
    ios-release)
        check_ios_release
        ;;
    ios-device)
        check_ios_device
        ;;
    ios-upload)
        check_ios_upload
        ;;
    all)
        check_macos_notarized
        check_android_release
        check_android_play
        check_ios_release
        check_ios_device
        check_ios_upload
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

echo
if [[ "${failures}" -eq 0 ]]; then
    echo "[preflight] ${target} prerequisites present"
else
    echo "[preflight] ${target} missing ${failures} prerequisite(s)"
    exit 1
fi
