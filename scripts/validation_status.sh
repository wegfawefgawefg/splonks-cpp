#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
strict=0
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
expected_revision="${SPLONKS_VALIDATION_REVISION:-}"

usage() {
    cat >&2 <<EOF
Usage: $0 [--strict]

Prints the current local evidence status for the Splonks onboarding and release
distribution goal. With --strict, exits nonzero until every completion criterion
has recorded evidence.

Environment:
  SPLONKS_RELEASE_VERSION   Release version in artifact names, default: ${version}
  SPLONKS_VALIDATION_REVISION
                            Git revision evidence must match, default: current HEAD
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

log_revision_matches() {
    local path="$1"
    grep -Fxq "git_revision=${expected_revision}" "${path}"
}

print_log_revision_check() {
    local label="$1"
    local pattern="$2"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    if log_revision_matches "${latest}"; then
        printf '[ok]      %s: %s has git_revision=%s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${expected_revision}"
        return 0
    fi
    local actual
    actual="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
    printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
        "${label}" \
        "${latest#${repo_root}/}" \
        "${actual:-<unset>}" \
        "${expected_revision}"
    return 1
}

print_log_revision_contains_check() {
    local label="$1"
    local pattern="$2"
    local required_text="$3"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    if ! log_revision_matches "${latest}"; then
        local actual_revision
        actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if grep -Fq "${required_text}" "${latest}"; then
        printf '[ok]      %s: %s has git_revision=%s and "%s"\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${expected_revision}" \
            "${required_text}"
        return 0
    fi
    printf '[missing] %s: %s missing required text: %s\n' \
        "${label}" \
        "${latest#${repo_root}/}" \
        "${required_text}"
    return 1
}

print_log_version_check() {
    local label="$1"
    local pattern="$2"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if grep -Fxq "release_version=${version}" "${latest}"; then
        printf '[ok]      %s: %s has release_version=%s git_revision=%s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${version}" \
            "${expected_revision}"
        return 0
    fi
    local actual
    actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${latest}" | tail -n 1)"
    printf '[missing] %s: %s has release_version=%s, expected %s\n' "${label}" "${latest#${repo_root}/}" "${actual:-<unset>}" "${version}"
    return 1
}

print_log_version_contains_check() {
    local label="$1"
    local pattern="$2"
    local required_text="$3"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if ! grep -Fxq "release_version=${version}" "${latest}"; then
        local actual
        actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${latest}" | tail -n 1)"
        printf '[missing] %s: %s has release_version=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual:-<unset>}" \
            "${version}"
        return 1
    fi
    if grep -Fq "${required_text}" "${latest}"; then
        printf '[ok]      %s: %s has release_version=%s git_revision=%s and "%s"\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${version}" \
            "${expected_revision}" \
            "${required_text}"
        return 0
    fi
    printf '[missing] %s: %s missing required text: %s\n' \
        "${label}" \
        "${latest#${repo_root}/}" \
        "${required_text}"
    return 1
}

print_log_version_check_any() {
    local label="$1"
    shift
    local pattern
    local latest=""
    local latest_mtime=0
    local candidate
    local mtime
    for pattern in "$@"; do
        if has_glob "${pattern}"; then
            while IFS= read -r candidate; do
                mtime="$(stat -c '%Y' "${candidate}" 2>/dev/null || stat -f '%m' "${candidate}" 2>/dev/null || echo 0)"
                if [[ -z "${latest}" || "${mtime}" -gt "${latest_mtime}" ]]; then
                    latest="${candidate}"
                    latest_mtime="${mtime}"
                fi
            done < <(ls -t ${pattern} 2>/dev/null)
        fi
    done
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s:\n' "${label}"
        for pattern in "$@"; do
            printf '          %s\n' "${pattern#${repo_root}/}"
        done
        return 1
    fi
    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${latest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if grep -Fxq "release_version=${version}" "${latest}"; then
        printf '[ok]      %s: %s has release_version=%s git_revision=%s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${version}" \
            "${expected_revision}"
        return 0
    fi
    local actual
    actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${latest}" | tail -n 1)"
    printf '[missing] %s: %s has release_version=%s, expected %s\n' "${label}" "${latest#${repo_root}/}" "${actual:-<unset>}" "${version}"
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

sha256_of_file() {
    local path="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "${path}" | awk '{print tolower($1)}'
    else
        shasum -a 256 "${path}" | awk '{print tolower($1)}'
    fi
}

print_checksum_match_check() {
    local label="$1"
    local artifact_path="$2"
    local checksum_path="$3"
    local expected
    local actual
    if [[ ! -f "${artifact_path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${artifact_path#${repo_root}/}"
        return 1
    fi
    if [[ ! -f "${checksum_path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${checksum_path#${repo_root}/}"
        return 1
    fi
    expected="$(awk 'NF >= 1 {print tolower($1); exit}' "${checksum_path}")"
    actual="$(sha256_of_file "${artifact_path}")"
    if [[ -n "${expected}" && "${actual}" == "${expected}" ]]; then
        printf '[ok]      %s: %s matches %s\n' \
            "${label}" \
            "${artifact_path#${repo_root}/}" \
            "${checksum_path#${repo_root}/}"
        return 0
    fi
    printf '[missing] %s: %s sha256=%s, expected %s\n' \
        "${label}" \
        "${artifact_path#${repo_root}/}" \
        "${actual:-<unset>}" \
        "${expected:-<unset>}"
    return 1
}

print_manifest_sha256_check() {
    local label="$1"
    local artifact_path="$2"
    local manifest_path="$3"
    local expected
    local actual
    if [[ ! -f "${artifact_path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${artifact_path#${repo_root}/}"
        return 1
    fi
    if [[ ! -f "${manifest_path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${manifest_path#${repo_root}/}"
        return 1
    fi
    expected="$(awk -F= '$1 == "sha256" {print tolower(substr($0, length("sha256") + 2))}' "${manifest_path}" | tail -n 1)"
    actual="$(sha256_of_file "${artifact_path}")"
    if [[ -n "${expected}" && "${actual}" == "${expected}" ]]; then
        printf '[ok]      %s: %s matches manifest sha256\n' \
            "${label}" \
            "${artifact_path#${repo_root}/}"
        return 0
    fi
    printf '[missing] %s: %s sha256=%s, expected %s\n' \
        "${label}" \
        "${artifact_path#${repo_root}/}" \
        "${actual:-<unset>}" \
        "${expected:-<unset>}"
    return 1
}

print_verified_bundle_check() {
    local label="$1"
    local pattern="$2"
    local latest
    latest="$(latest_glob "${pattern}" || true)"
    if [[ -z "${latest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi

    local work_dir
    local bundle_dir
    work_dir="$(mktemp -d)"
    if ! tar -C "${work_dir}" -xzf "${latest}" >/dev/null 2>&1; then
        rm -rf "${work_dir}"
        printf '[missing] %s: %s could not be extracted\n' "${label}" "${latest#${repo_root}/}"
        return 1
    fi
    bundle_dir="$(find "${work_dir}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
    if [[ -z "${bundle_dir}" ||
        ! -f "${bundle_dir}/BUNDLE_MANIFEST.txt" ||
        ! -f "${bundle_dir}/CHECKSUMS.sha256" ]]; then
        rm -rf "${work_dir}"
        printf '[missing] %s: %s missing BUNDLE_MANIFEST.txt or CHECKSUMS.sha256\n' \
            "${label}" \
            "${latest#${repo_root}/}"
        return 1
    fi
    if ! grep -Fxq "release_version=${version}" "${bundle_dir}/BUNDLE_MANIFEST.txt"; then
        local actual
        actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${bundle_dir}/BUNDLE_MANIFEST.txt" | tail -n 1)"
        rm -rf "${work_dir}"
        printf '[missing] %s: %s has release_version=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual:-<unset>}" \
            "${version}"
        return 1
    fi
    if ! grep -Fxq "git_revision=${expected_revision}" "${bundle_dir}/BUNDLE_MANIFEST.txt"; then
        local actual_revision
        actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${bundle_dir}/BUNDLE_MANIFEST.txt" | tail -n 1)"
        rm -rf "${work_dir}"
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if command -v sha256sum >/dev/null 2>&1; then
        if ! (cd "${bundle_dir}" && sha256sum -c CHECKSUMS.sha256 >/dev/null); then
            rm -rf "${work_dir}"
            printf '[missing] %s: %s failed CHECKSUMS.sha256 verification\n' \
                "${label}" \
                "${latest#${repo_root}/}"
            return 1
        fi
    else
        if ! (cd "${bundle_dir}" && shasum -a 256 -c CHECKSUMS.sha256 >/dev/null); then
            rm -rf "${work_dir}"
            printf '[missing] %s: %s failed CHECKSUMS.sha256 verification\n' \
                "${label}" \
                "${latest#${repo_root}/}"
            return 1
        fi
    fi
    rm -rf "${work_dir}"
    printf '[ok]      %s: %s release_version=%s git_revision=%s and checksums verified\n' \
        "${label}" \
        "${latest#${repo_root}/}" \
        "${version}" \
        "${expected_revision}"
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

print_manifest_revision_check() {
    local label="$1"
    local path="$2"
    local key="$3"
    local actual
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi
    actual="$(awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${path}" | tail -n 1)"
    if [[ "${actual}" == "${expected_revision}" ]]; then
        printf '[ok]      %s: %s has %s=%s\n' \
            "${label}" \
            "${path#${repo_root}/}" \
            "${key}" \
            "${expected_revision}"
        return 0
    fi
    printf '[missing] %s: %s has %s=%s, expected %s\n' \
        "${label}" \
        "${path#${repo_root}/}" \
        "${key}" \
        "${actual:-<unset>}" \
        "${expected_revision}"
    return 1
}

failures=0
check() {
    "$@" || failures=$((failures + 1))
}

cd "${repo_root}"
if [[ -z "${expected_revision}" ]]; then
    expected_revision="$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
fi

echo "Splonks validation status"
echo "git_revision=$(git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
echo "release_version=${version}"
echo "validation_revision=${expected_revision}"
echo

echo "[desktop]"
check print_log_revision_contains_check \
    "Linux dev validation" \
    "${repo_root}/dist/validation/linux-dev-*.log" \
    "[validated] linux developer build and headless smoke"
check print_log_version_contains_check \
    "Linux release validation" \
    "${repo_root}/dist/validation/linux-release-*.log" \
    "[validated] linux release package and archive"
check print_file_check "Linux release archive" "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz"
check print_checksum_match_check \
    "Linux release checksum" \
    "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz" \
    "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz.sha256"
check print_manifest_revision_check "Linux package revision" "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt" git_revision
check print_log_revision_contains_check \
    "macOS dev validation" \
    "${repo_root}/dist/validation/macos-dev-*.log" \
    "[validated] macos developer build and headless smoke"
check print_log_version_contains_check \
    "macOS release validation" \
    "${repo_root}/dist/validation/macos-release-*.log" \
    "[validated] macos release package and archive"
check print_log_version_contains_check \
    "macOS notarized validation" \
    "${repo_root}/dist/validation/macos-macos-notarized-*.log" \
    "[validated] macos Developer ID signed, notarized, stapled release archive"
check print_file_check "macOS release archive" "${repo_root}/dist/releases/splonks-${version}-macos-universal.zip"
check print_checksum_match_check \
    "macOS release checksum" \
    "${repo_root}/dist/releases/splonks-${version}-macos-universal.zip" \
    "${repo_root}/dist/releases/splonks-${version}-macos-universal.zip.sha256"
check print_log_revision_contains_check \
    "Windows dev validation" \
    "${repo_root}/dist/validation/windows-dev-*.log" \
    "[validated] windows developer build and headless smoke"
check print_log_version_contains_check \
    "Windows release validation" \
    "${repo_root}/dist/validation/windows-release-*.log" \
    "[validated] windows release package and archive"
check print_file_check "Windows release archive" "${repo_root}/dist/releases/splonks-${version}-windows-x86_64.zip"
check print_checksum_match_check \
    "Windows release checksum" \
    "${repo_root}/dist/releases/splonks-${version}-windows-x86_64.zip" \
    "${repo_root}/dist/releases/splonks-${version}-windows-x86_64.zip.sha256"
echo

echo "[android]"
check print_log_revision_contains_check \
    "Android emulator/dev validation" \
    "${repo_root}/dist/validation/*-android-emulator-*.log" \
    "state fingerprint smoke ok"
check print_log_version_check "Android signed AAB validation" "${repo_root}/dist/validation/*-android-release-*.log"
check print_file_check "Android release AAB" "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab"
check print_file_check "Android release manifest" "${repo_root}/dist/splonks-android/manifest.txt"
check print_manifest_value_check "Android manifest version" "${repo_root}/dist/splonks-android/manifest.txt" version_name "${version}"
check print_manifest_revision_check "Android manifest revision" "${repo_root}/dist/splonks-android/manifest.txt" git_commit
check print_manifest_sha256_check \
    "Android AAB checksum" \
    "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab" \
    "${repo_root}/dist/splonks-android/manifest.txt"
check print_log_version_check_any "Android Play upload validation" \
    "${repo_root}/dist/validation/*-android-play-upload-*.log" \
    "${repo_root}/dist/validation/android-play-*.log"
echo

echo "[ios]"
check print_log_revision_contains_check \
    "iOS simulator validation" \
    "${repo_root}/dist/validation/macos-ios-sim-*.log" \
    "state fingerprint smoke ok"
check print_log_version_check "iOS release archive validation" "${repo_root}/dist/validation/macos-ios-release-*.log"
check print_file_check "iOS IPA" "${repo_root}/dist/releases/splonks-${version}-ios.ipa"
check print_checksum_match_check \
    "iOS IPA checksum" \
    "${repo_root}/dist/releases/splonks-${version}-ios.ipa" \
    "${repo_root}/dist/releases/splonks-${version}-ios.ipa.sha256"
check print_file_check "iOS release manifest" "${repo_root}/dist/splonks-ios/manifest.txt"
check print_manifest_value_check "iOS manifest version" "${repo_root}/dist/splonks-ios/manifest.txt" version_name "${version}"
check print_manifest_revision_check "iOS manifest revision" "${repo_root}/dist/splonks-ios/manifest.txt" git_commit
check print_manifest_sha256_check \
    "iOS manifest checksum" \
    "${repo_root}/dist/releases/splonks-${version}-ios.ipa" \
    "${repo_root}/dist/splonks-ios/manifest.txt"
check print_log_version_check_any "iOS App Store/TestFlight upload validation" \
    "${repo_root}/dist/validation/macos-ios-upload-*.log" \
    "${repo_root}/dist/validation/ios-upload-*.log"
echo

echo "[handoff]"
check print_verified_bundle_check "Validation evidence bundle" "${repo_root}/dist/validation-bundles/splonks-validation-*.tar.gz"
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
