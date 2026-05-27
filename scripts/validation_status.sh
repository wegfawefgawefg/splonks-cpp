#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
strict=0
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"

usage() {
    cat >&2 <<EOF
Usage: $0 [--strict]

Prints the current local evidence status for the Splonks onboarding and release
distribution goal. With --strict, exits nonzero until every completion criterion
has recorded evidence.

Environment:
  SPLONKS_RELEASE_VERSION   Release version in artifact names, default: ${version}
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --strict)
            strict=1
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

has_glob() {
    compgen -G "$1" >/dev/null
}

latest_glob() {
    local pattern="$1"
    if has_glob "${pattern}"; then
        ls -t ${pattern} 2>/dev/null | head -n 1
    fi
}

print_check() {
    local label="$1"
    local pattern="$2"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -n "${latest}" ]]; then
        printf '[ok]      %s: %s\n' "${label}" "${latest#${repo_root}/}"
        return 0
    fi
    printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
    return 1
}

print_file_check() {
    local label="$1"
    local path="$2"
    if [[ -f "${path}" ]]; then
        printf '[ok]      %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 0
    fi
    printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
    return 1
}

print_manifest_value_check() {
    local label="$1"
    local path="$2"
    local key="$3"
    local expected="$4"
    local actual
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi
    actual="$(awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${path}" | tail -n 1)"
    if [[ "${actual}" == "${expected}" ]]; then
        printf '[ok]      %s: %s has %s=%s\n' "${label}" "${path#${repo_root}/}" "${key}" "${expected}"
        return 0
    fi
    printf '[missing] %s: %s has %s=%s, expected %s\n' "${label}" "${path#${repo_root}/}" "${key}" "${actual:-<unset>}" "${expected}"
    return 1
}

failures=0
check() {
    "$@" || failures=$((failures + 1))
}

cd "${repo_root}"

echo "Splonks validation status"
echo "git_revision=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
echo "release_version=${version}"
echo

echo "[desktop]"
check print_check "Linux dev validation" "${repo_root}/dist/validation/linux-dev-*.log"
check print_check "Linux release validation" "${repo_root}/dist/validation/linux-release-*.log"
check print_file_check "Linux release archive" "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz"
check print_file_check "Linux release checksum" "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz.sha256"
check print_check "macOS dev validation" "${repo_root}/dist/validation/macos-dev-*.log"
check print_check "macOS release validation" "${repo_root}/dist/validation/macos-release-*.log"
check print_check "macOS notarized validation" "${repo_root}/dist/validation/macos-macos-notarized-*.log"
check print_check "Windows dev validation" "${repo_root}/dist/validation/windows-dev-*.log"
check print_check "Windows release validation" "${repo_root}/dist/validation/windows-release-*.log"
echo

echo "[android]"
check print_check "Android emulator/dev validation" "${repo_root}/dist/validation/*-android-emulator-*.log"
check print_check "Android signed AAB validation" "${repo_root}/dist/validation/*-android-release-*.log"
check print_file_check "Android release AAB" "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab"
check print_file_check "Android release manifest" "${repo_root}/dist/splonks-android/manifest.txt"
check print_manifest_value_check "Android manifest version" "${repo_root}/dist/splonks-android/manifest.txt" version_name "${version}"
check print_check "Android Play upload validation" "${repo_root}/dist/validation/*-android-play-upload-*.log"
echo

echo "[ios]"
check print_check "iOS simulator validation" "${repo_root}/dist/validation/macos-ios-sim-*.log"
check print_check "iOS release archive validation" "${repo_root}/dist/validation/macos-ios-release-*.log"
check print_file_check "iOS IPA" "${repo_root}/dist/releases/splonks-${version}-ios.ipa"
check print_file_check "iOS IPA checksum" "${repo_root}/dist/releases/splonks-${version}-ios.ipa.sha256"
check print_file_check "iOS release manifest" "${repo_root}/dist/splonks-ios/manifest.txt"
check print_manifest_value_check "iOS manifest version" "${repo_root}/dist/splonks-ios/manifest.txt" version_name "${version}"
check print_check "iOS App Store/TestFlight upload validation" "${repo_root}/dist/validation/macos-ios-upload-*.log"
echo

echo "[handoff]"
check print_check "Validation evidence bundle" "${repo_root}/dist/validation-bundles/splonks-validation-*.tar.gz"
echo

echo "[ci]"
if grep -Eq '^  pull_request:|branches:' .github/workflows/package.yml; then
    echo "[missing] Package workflow appears to include PR or branch triggers"
    failures=$((failures + 1))
else
    echo "[ok]      Package workflow has no PR or branch trigger patterns"
fi
if grep -Eq '^  workflow_dispatch:' .github/workflows/package.yml && grep -Eq "^      - 'v\\*'" .github/workflows/package.yml; then
    echo "[ok]      Package workflow is manual/tag based"
else
    echo "[missing] Package workflow manual/tag trigger evidence"
    failures=$((failures + 1))
fi

echo
if [[ "${failures}" -eq 0 ]]; then
    echo "[status] complete evidence present"
else
    echo "[status] ${failures} missing evidence item(s)"
fi

if [[ "${strict}" -eq 1 && "${failures}" -ne 0 ]]; then
    exit 1
fi
