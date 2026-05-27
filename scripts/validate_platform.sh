#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
scope="${1:-all}"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
validation_dir="${repo_root}/dist/validation"

usage() {
    cat >&2 <<EOF
Usage: $0 [dev|release|all|macos-notarized|android-dev|android-emulator|android-release|android-play-upload|ios-sim|ios-release|ios-upload]

Runs the platform validation commands and writes a timestamped evidence log.
Run the platform setup script first when validating a fresh developer machine.

Environment:
  SPLONKS_RELEASE_VERSION   Release version for archives, default: ${version}
EOF
}

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

require_host() {
    local want="$1"
    local got="$2"
    if [[ "${got}" != "${want}" ]]; then
        echo "${scope} validation must run on ${want}; current host is ${got}" >&2
        exit 1
    fi
}

run_step() {
    echo
    echo "[validate] $*"
    "$@"
}

print_first_line() {
    local label="$1"
    shift
    local cmd="$1"
    local output
    local first_line
    if command -v "${cmd}" >/dev/null 2>&1; then
        output="$("$@" 2>&1 || true)"
        IFS= read -r first_line <<< "${output}"
        echo "${label}=${first_line}"
    fi
}

print_all_lines() {
    local label="$1"
    shift
    local cmd="$1"
    if command -v "${cmd}" >/dev/null 2>&1; then
        echo "${label}<<EOF"
        "$@" 2>&1
        echo "EOF"
    fi
}

write_environment() {
    local platform="$1"
    echo "[environment]"
    echo "platform=${platform}"
    echo "scope=${scope}"
    echo "release_version=${version}"
    echo "timestamp_utc=${timestamp}"
    echo "git_revision=$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
    echo "uname=$(uname -a)"
    echo "msystem=${MSYSTEM:-}"
    echo "path=${PATH}"
    print_first_line cmake_version cmake --version
    print_first_line ninja_version ninja --version
    print_first_line pkg_config_version pkg-config --version
    print_first_line git_version git --version
    print_first_line cc_version cc --version
    print_first_line cxx_version c++ --version
    print_first_line gcc_version gcc --version
    print_first_line gxx_version g++ --version
    print_first_line clang_version clang --version
    print_first_line clangxx_version clang++ --version
    print_first_line brew_version brew --version
    print_first_line pacman_version pacman --version
    print_first_line java_version java -version
    print_first_line adb_version adb version
    print_first_line sdkmanager_version sdkmanager --version
    print_first_line xcrun_version xcrun --version
    if command -v xcode-select >/dev/null 2>&1; then
        echo "xcode_select_path=$(xcode-select -p 2>/dev/null || true)"
    fi
    print_all_lines xcodebuild_version xcodebuild -version
}

validate_desktop_dev() {
    local platform="$1"
    run_step env SPLONKS_PRESET=dev "${repo_root}/scripts/verify_dev_env.sh"
    echo "[validated] ${platform} developer build and headless smoke"
}

validate_desktop_release() {
    local platform="$1"
    case "${platform}" in
        linux)
            run_step "${repo_root}/scripts/package_linux.sh"
            run_step "${repo_root}/scripts/verify_package_linux.sh"
            ;;
        macos)
            run_step "${repo_root}/scripts/package_macos.sh"
            run_step "${repo_root}/scripts/verify_package_macos.sh"
            ;;
        windows)
            run_step "${repo_root}/scripts/package_windows.sh"
            run_step "${repo_root}/scripts/verify_package_windows.sh"
            ;;
        *)
            echo "No desktop release validation for platform: ${platform}" >&2
            exit 1
            ;;
    esac
    run_step env SPLONKS_RELEASE_VERSION="${version}" "${repo_root}/scripts/archive_release.sh" "${platform}"
    run_step env SPLONKS_RELEASE_VERSION="${version}" "${repo_root}/scripts/verify_release_archive.sh" "${platform}"
    echo "[validated] ${platform} release package and archive"
}

validate_macos_notarized() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/package_macos.sh"
    run_step "${repo_root}/scripts/verify_package_macos.sh"
    run_step "${repo_root}/scripts/macos/notarize_app.sh"
    run_step env SPLONKS_RELEASE_VERSION="${version}" "${repo_root}/scripts/verify_release_archive.sh" macos
    echo "[validated] macos Developer ID signed, notarized, stapled, quarantined release archive"
}

validate_android_dev() {
    run_step "${repo_root}/scripts/android/setup_sdk.sh"
    run_step "${repo_root}/scripts/android/fetch_sdl3_aar.sh"
    run_step env SPLONKS_ANDROID_ABIS="${SPLONKS_ANDROID_ABIS:-x86_64}" "${repo_root}/scripts/android/build_apk.sh"
    run_step "${repo_root}/scripts/android/install_apk.sh"
    run_step "${repo_root}/scripts/android/run_smoke.sh"
    echo "[validated] android debug APK install and runtime smoke"
}

validate_android_emulator() {
    run_step "${repo_root}/scripts/android/setup_sdk.sh"
    run_step "${repo_root}/scripts/android/fetch_sdl3_aar.sh"
    run_step env SPLONKS_ANDROID_ABIS="${SPLONKS_ANDROID_ABIS:-x86_64}" "${repo_root}/scripts/android/build_apk.sh"
    run_step "${repo_root}/scripts/android/create_avd.sh"
    run_step "${repo_root}/scripts/android/start_emulator.sh"
    run_step "${repo_root}/scripts/android/install_apk.sh"
    run_step "${repo_root}/scripts/android/run_smoke.sh"
    echo "[validated] android emulator debug APK install and runtime smoke"
}

validate_android_release() {
    run_step "${repo_root}/scripts/android/setup_sdk.sh"
    run_step "${repo_root}/scripts/android/fetch_sdl3_aar.sh"
    run_step "${repo_root}/scripts/android/build_release_aab.sh"
    run_step "${repo_root}/scripts/android/verify_release_aab.sh"
    echo "[validated] android signed release AAB"
}

validate_android_play_upload() {
    run_step "${repo_root}/scripts/android/upload_play.sh"
    echo "[validated] android Google Play upload path"
}

validate_ios_sim() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/ios/run_sim.sh" --check-state-fingerprint-smoke
    echo "[validated] ios simulator build, install, launch, and runtime smoke"
}

validate_ios_release() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/ios/archive_release.sh"
    echo "[validated] ios signed archive/export"
}

validate_ios_upload() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/ios/archive_release.sh"
    run_step "${repo_root}/scripts/ios/upload_app_store.sh" validate-upload
    echo "[validated] ios App Store Connect upload"
}

case "${scope}" in
    dev|release|all|macos-notarized|android-dev|android-emulator|android-release|android-play-upload|ios-sim|ios-release|ios-upload) ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac

platform="$(host_platform)"
mkdir -p "${validation_dir}"
log_path="${validation_dir}/${platform}-${scope}-${timestamp}.log"

(
    cd "${repo_root}"
    write_environment "${platform}"
    case "${scope}" in
        dev)
            validate_desktop_dev "${platform}"
            ;;
        release)
            validate_desktop_release "${platform}"
            ;;
        all)
            validate_desktop_dev "${platform}"
            validate_desktop_release "${platform}"
            if [[ "${platform}" == "macos" ]]; then
                validate_ios_sim "${platform}"
            fi
            ;;
        macos-notarized)
            validate_macos_notarized "${platform}"
            ;;
        android-dev)
            validate_android_dev
            ;;
        android-emulator)
            validate_android_emulator
            ;;
        android-release)
            validate_android_release
            ;;
        android-play-upload)
            validate_android_play_upload
            ;;
        ios-sim)
            validate_ios_sim "${platform}"
            ;;
        ios-release)
            validate_ios_release "${platform}"
            ;;
        ios-upload)
            validate_ios_upload "${platform}"
            ;;
    esac
    echo
    echo "[validate] evidence log: ${log_path}"
) 2>&1 | tee "${log_path}"

echo "[validate] wrote ${log_path}"
