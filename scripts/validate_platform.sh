#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
scope="${1:-all}"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
validation_dir="${repo_root}/dist/validation"

usage() {
    cat >&2 <<EOF
Usage: $0 [dev|release|all|android-dev|android-emulator|android-release|ios-sim|ios-release]

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

write_environment() {
    local platform="$1"
    echo "[environment]"
    echo "platform=${platform}"
    echo "scope=${scope}"
    echo "release_version=${version}"
    echo "timestamp_utc=${timestamp}"
    echo "git_revision=$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
    echo "uname=$(uname -a)"
    if command -v cmake >/dev/null 2>&1; then
        cmake --version | head -n 1
    fi
    if command -v git >/dev/null 2>&1; then
        git --version
    fi
    if command -v xcodebuild >/dev/null 2>&1; then
        xcodebuild -version
    fi
    if command -v java >/dev/null 2>&1; then
        java -version 2>&1 | head -n 1
    fi
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
    echo "[validated] android signed release AAB"
}

validate_ios_sim() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/ios/build_sim.sh"
    run_step "${repo_root}/scripts/ios/run_sim.sh"
    echo "[validated] ios simulator build and launch"
}

validate_ios_release() {
    local platform="$1"
    require_host macos "${platform}"
    run_step "${repo_root}/scripts/ios/archive_release.sh"
    echo "[validated] ios signed archive/export"
}

case "${scope}" in
    dev|release|all|android-dev|android-emulator|android-release|ios-sim|ios-release) ;;
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
        android-dev)
            validate_android_dev
            ;;
        android-emulator)
            validate_android_emulator
            ;;
        android-release)
            validate_android_release
            ;;
        ios-sim)
            validate_ios_sim "${platform}"
            ;;
        ios-release)
            validate_ios_release "${platform}"
            ;;
    esac
    echo
    echo "[validate] evidence log: ${log_path}"
) 2>&1 | tee "${log_path}"

echo "[validate] wrote ${log_path}"
