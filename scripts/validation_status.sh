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

file_mtime() {
    stat -c '%Y' "$1" 2>/dev/null || stat -f '%m' "$1" 2>/dev/null || echo 0
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
    local latest=""
    local newest=""
    local newest_mtime=0
    local candidate
    local mtime
    if has_glob "${pattern}"; then
        while IFS= read -r candidate; do
            mtime="$(file_mtime "${candidate}")"
            if [[ -z "${newest}" || "${mtime}" -gt "${newest_mtime}" ]]; then
                newest="${candidate}"
                newest_mtime="${mtime}"
            fi
            if log_revision_matches "${candidate}" && grep -Fq "${required_text}" "${candidate}"; then
                if [[ -z "${latest}" || "${mtime}" -gt "$(file_mtime "${latest}")" ]]; then
                    latest="${candidate}"
                fi
            fi
        done < <(ls -t ${pattern} 2>/dev/null)
    fi
    if [[ -n "${latest}" ]]; then
        printf '[ok]      %s: %s has git_revision=%s and "%s"\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${expected_revision}" \
            "${required_text}"
        return 0
    fi
    if [[ -z "${newest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    if ! log_revision_matches "${newest}"; then
        local actual_revision
        actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${newest}" | tail -n 1)"
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${newest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    printf '[missing] %s: %s missing required text: %s\n' \
        "${label}" \
        "${newest#${repo_root}/}" \
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
    local latest=""
    local newest=""
    local newest_mtime=0
    local candidate
    local mtime
    if has_glob "${pattern}"; then
        while IFS= read -r candidate; do
            mtime="$(file_mtime "${candidate}")"
            if [[ -z "${newest}" || "${mtime}" -gt "${newest_mtime}" ]]; then
                newest="${candidate}"
                newest_mtime="${mtime}"
            fi
            if grep -Fxq "git_revision=${expected_revision}" "${candidate}" &&
                grep -Fxq "release_version=${version}" "${candidate}" &&
                grep -Fq "${required_text}" "${candidate}"; then
                if [[ -z "${latest}" || "${mtime}" -gt "$(file_mtime "${latest}")" ]]; then
                    latest="${candidate}"
                fi
            fi
        done < <(ls -t ${pattern} 2>/dev/null)
    fi
    if [[ -n "${latest}" ]]; then
        printf '[ok]      %s: %s has release_version=%s git_revision=%s and "%s"\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${version}" \
            "${expected_revision}" \
            "${required_text}"
        return 0
    fi
    if [[ -z "${newest}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${pattern#${repo_root}/}"
        return 1
    fi
    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${newest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${newest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if ! grep -Fxq "release_version=${version}" "${newest}"; then
        local actual
        actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${newest}" | tail -n 1)"
        printf '[missing] %s: %s has release_version=%s, expected %s\n' \
            "${label}" \
            "${newest#${repo_root}/}" \
            "${actual:-<unset>}" \
            "${version}"
        return 1
    fi
    printf '[missing] %s: %s missing required text: %s\n' \
        "${label}" \
        "${newest#${repo_root}/}" \
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

print_log_version_contains_check_any() {
    local label="$1"
    local required_text="$2"
    shift 2
    local pattern
    local latest=""
    local latest_mtime=0
    local newest=""
    local newest_mtime=0
    local candidate
    local mtime
    for pattern in "$@"; do
        if has_glob "${pattern}"; then
            while IFS= read -r candidate; do
                mtime="$(file_mtime "${candidate}")"
                if [[ -z "${newest}" || "${mtime}" -gt "${newest_mtime}" ]]; then
                    newest="${candidate}"
                    newest_mtime="${mtime}"
                fi
                if grep -Fxq "git_revision=${expected_revision}" "${candidate}" &&
                    grep -Fxq "release_version=${version}" "${candidate}" &&
                    grep -Fq "${required_text}" "${candidate}"; then
                    if [[ -z "${latest}" || "${mtime}" -gt "${latest_mtime}" ]]; then
                        latest="${candidate}"
                        latest_mtime="${mtime}"
                    fi
                fi
            done < <(ls -t ${pattern} 2>/dev/null)
        fi
    done
    if [[ -n "${latest}" ]]; then
        printf '[ok]      %s: %s has release_version=%s git_revision=%s and "%s"\n' \
            "${label}" \
            "${latest#${repo_root}/}" \
            "${version}" \
            "${expected_revision}" \
            "${required_text}"
        return 0
    fi
    if [[ -z "${newest}" ]]; then
        printf '[missing] %s:\n' "${label}"
        for pattern in "$@"; do
            printf '          %s\n' "${pattern#${repo_root}/}"
        done
        return 1
    fi
    local actual_revision
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${newest}" | tail -n 1)"
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        printf '[missing] %s: %s has git_revision=%s, expected %s\n' \
            "${label}" \
            "${newest#${repo_root}/}" \
            "${actual_revision:-<unset>}" \
            "${expected_revision}"
        return 1
    fi
    if ! grep -Fxq "release_version=${version}" "${newest}"; then
        local actual
        actual="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${newest}" | tail -n 1)"
        printf '[missing] %s: %s has release_version=%s, expected %s\n' \
            "${label}" \
            "${newest#${repo_root}/}" \
            "${actual:-<unset>}" \
            "${version}"
        return 1
    fi
    printf '[missing] %s: %s missing required text: %s\n' \
        "${label}" \
        "${newest#${repo_root}/}" \
        "${required_text}"
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

print_manifest_value_in_check() {
    local label="$1"
    local path="$2"
    local key="$3"
    shift 3
    local actual
    local expected
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi
    actual="$(awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${path}" | tail -n 1)"
    for expected in "$@"; do
        if [[ "${actual}" == "${expected}" ]]; then
            printf '[ok]      %s: %s has %s=%s\n' "${label}" "${path#${repo_root}/}" "${key}" "${actual}"
            return 0
        fi
    done
    printf '[missing] %s: %s has %s=%s, expected one of: %s\n' \
        "${label}" \
        "${path#${repo_root}/}" \
        "${key}" \
        "${actual:-<unset>}" \
        "$*"
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

print_package_manifest_check() {
    local label="$1"
    local path="$2"
    local platform="$3"
    local app
    local actual_platform
    local mode
    local actual_version
    local actual_revision
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi

    app="$(awk -F= '$1 == "app" {print substr($0, length("app") + 2)}' "${path}" | tail -n 1)"
    actual_platform="$(awk -F= '$1 == "platform" {print substr($0, length("platform") + 2)}' "${path}" | tail -n 1)"
    mode="$(awk -F= '$1 == "mode" {print substr($0, length("mode") + 2)}' "${path}" | tail -n 1)"
    actual_version="$(awk -F= '$1 == "release_version" {print substr($0, length("release_version") + 2)}' "${path}" | tail -n 1)"
    actual_revision="$(awk -F= '$1 == "git_revision" {print substr($0, length("git_revision") + 2)}' "${path}" | tail -n 1)"

    if [[ "${app}" == "splonks" &&
        "${actual_platform}" == "${platform}" &&
        "${mode}" == "release" &&
        "${actual_version}" == "${version}" &&
        "${actual_revision}" == "${expected_revision}" ]]; then
        printf '[ok]      %s: %s platform=%s release_version=%s git_revision=%s\n' \
            "${label}" \
            "${path#${repo_root}/}" \
            "${platform}" \
            "${version}" \
            "${expected_revision}"
        return 0
    fi

    printf '[missing] %s: %s has app=%s platform=%s mode=%s release_version=%s git_revision=%s, expected app=splonks platform=%s mode=release release_version=%s git_revision=%s\n' \
        "${label}" \
        "${path#${repo_root}/}" \
        "${app:-<unset>}" \
        "${actual_platform:-<unset>}" \
        "${mode:-<unset>}" \
        "${actual_version:-<unset>}" \
        "${actual_revision:-<unset>}" \
        "${platform}" \
        "${version}" \
        "${expected_revision}"
    return 1
}

failures=0
check() {
    "$@" || failures=$((failures + 1))
}

check_workflow_contains() {
    local label="$1"
    local required="$2"
    if grep -Fq "${required}" "${repo_root}/.github/workflows/package.yml"; then
        printf '[ok]      %s: package workflow contains "%s"\n' "${label}" "${required}"
        return 0
    fi
    printf '[missing] %s: package workflow missing "%s"\n' "${label}" "${required}"
    return 1
}

check_workflow_not_contains() {
    local label="$1"
    local denied="$2"
    if grep -Fq "${denied}" "${repo_root}/.github/workflows/package.yml"; then
        printf '[missing] %s: package workflow still contains "%s"\n' "${label}" "${denied}"
        return 1
    fi
    printf '[ok]      %s: package workflow does not contain "%s"\n' "${label}" "${denied}"
    return 0
}

check_file_contains() {
    local label="$1"
    local path="$2"
    local required="$3"
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi
    if grep -Fq "${required}" "${path}"; then
        printf '[ok]      %s: %s contains "%s"\n' "${label}" "${path#${repo_root}/}" "${required}"
        return 0
    fi
    printf '[missing] %s: %s missing "%s"\n' "${label}" "${path#${repo_root}/}" "${required}"
    return 1
}

check_file_not_contains() {
    local label="$1"
    local path="$2"
    local denied="$3"
    if [[ ! -f "${path}" ]]; then
        printf '[missing] %s: %s\n' "${label}" "${path#${repo_root}/}"
        return 1
    fi
    if grep -Fq "${denied}" "${path}"; then
        printf '[missing] %s: %s still contains "%s"\n' "${label}" "${path#${repo_root}/}" "${denied}"
        return 1
    fi
    printf '[ok]      %s: %s does not contain "%s"\n' "${label}" "${path#${repo_root}/}" "${denied}"
    return 0
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
check print_package_manifest_check "Linux package manifest" "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt" linux
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
    "[validated] macos Developer ID signed, notarized, stapled, quarantined release archive"
check print_file_check "macOS release archive" "${repo_root}/dist/releases/splonks-${version}-macos-arm64.zip"
check print_checksum_match_check \
    "macOS release checksum" \
    "${repo_root}/dist/releases/splonks-${version}-macos-arm64.zip" \
    "${repo_root}/dist/releases/splonks-${version}-macos-arm64.zip.sha256"
check print_package_manifest_check "macOS package manifest" "${repo_root}/dist/splonks-macos/PACKAGE_MANIFEST.txt" macos
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
check print_package_manifest_check "Windows package manifest" "${repo_root}/dist/splonks-windows/PACKAGE_MANIFEST.txt" windows
echo

echo "[android]"
check print_log_revision_contains_check \
    "Android emulator/dev validation" \
    "${repo_root}/dist/validation/*-android-emulator-*.log" \
    "state fingerprint smoke ok"
check print_log_version_contains_check \
    "Android signed AAB validation" \
    "${repo_root}/dist/validation/*-android-release-*.log" \
    "[validated] android signed release AAB"
check print_log_version_contains_check \
    "Android upload-key AAB validation" \
    "${repo_root}/dist/validation/*-android-release-*.log" \
    "[validated] android signed release AAB with upload key"
check print_file_check "Android release AAB" "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab"
check print_file_check "Android release manifest" "${repo_root}/dist/splonks-android/manifest.txt"
check print_manifest_value_check "Android manifest version" "${repo_root}/dist/splonks-android/manifest.txt" version_name "${version}"
check print_manifest_revision_check "Android manifest revision" "${repo_root}/dist/splonks-android/manifest.txt" git_commit
check print_manifest_value_in_check \
    "Android manifest keystore purpose" \
    "${repo_root}/dist/splonks-android/manifest.txt" \
    keystore_purpose \
    validation \
    upload
check print_manifest_sha256_check \
    "Android AAB checksum" \
    "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab" \
    "${repo_root}/dist/splonks-android/manifest.txt"
check print_log_version_contains_check_any \
    "Android Play upload validation" \
    "[play-upload] upload complete" \
    "${repo_root}/dist/validation/*-android-play-upload-*.log" \
    "${repo_root}/dist/validation/android-play-*.log"
echo

echo "[ios]"
check print_log_revision_contains_check \
    "iOS simulator validation" \
    "${repo_root}/dist/validation/macos-ios-sim-*.log" \
    "state fingerprint smoke ok"
check print_log_version_contains_check \
    "iOS release archive validation" \
    "${repo_root}/dist/validation/macos-ios-release-*.log" \
    "[validated] ios signed archive/export"
check print_log_version_contains_check \
    "iOS device install validation" \
    "${repo_root}/dist/validation/macos-ios-device-*.log" \
    "[validated] ios device install and launch"
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
check print_log_version_contains_check_any \
    "iOS App Store/TestFlight upload validation" \
    "[ios-upload] validate-upload complete" \
    "${repo_root}/dist/validation/macos-ios-upload-*.log" \
    "${repo_root}/dist/validation/ios-upload-*.log"
echo

echo "[handoff]"
check print_verified_bundle_check "Validation evidence bundle" "${repo_root}/dist/validation-bundles/splonks-validation-*.tar.gz"
check check_file_contains "Evidence bundle filters stale manifests" "${repo_root}/scripts/bundle_validation_evidence.sh" "skip_stale_manifest"
check check_file_contains "Evidence importer checks bundle release version" "${repo_root}/scripts/import_validation_evidence.sh" "Evidence bundle release_version="
check check_file_contains "Evidence importer checks bundle revision" "${repo_root}/scripts/import_validation_evidence.sh" "Evidence bundle git_revision="
check check_file_contains "Evidence importer supports target checks" "${repo_root}/scripts/import_validation_evidence.sh" "SPLONKS_IMPORT_EXPECT_TARGET"
check check_file_contains "Evidence importer checks target proof lines" "${repo_root}/scripts/import_validation_evidence.sh" "require_bundle_file_contains"
check check_file_contains "Generated handoffs import pinned target evidence" "${repo_root}/scripts/print_validation_handoff.sh" 'SPLONKS_IMPORT_EXPECT_TARGET=${import_target}'
check check_file_contains "Generated handoffs use target-specific return notes" "${repo_root}/scripts/print_validation_handoff.sh" "Play validate/upload result"
check check_file_contains "Android Play handoff builds upload-key AAB" "${repo_root}/scripts/print_validation_handoff.sh" "Run this to build and verify the signed AAB with the real upload key"
check check_file_contains "Handoff packet writer pins validation revision" "${repo_root}/scripts/write_validation_handoff.sh" "SPLONKS_VALIDATION_REVISION"
check check_file_contains "Handoff packet filenames use requested validation revision" "${repo_root}/scripts/write_validation_handoff.sh" 'short_revision="${validation_revision:0:12}"'
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
check check_workflow_not_contains "Package workflow avoids moving macOS latest label" "macos-latest"
check check_workflow_contains "Package workflow uses explicit Apple Silicon macOS runner" "runs-on: macos-15"
check check_file_contains "macOS package preset is arm64-only" "${repo_root}/CMakePresets.json" '"CMAKE_OSX_ARCHITECTURES": "arm64"'
check check_file_contains "macOS setup rejects Intel hosts" "${repo_root}/scripts/setup_macos.sh" "Splonks supports Apple Silicon macOS development and release packaging only."
check check_file_contains "macOS dev verifier rejects Intel hosts" "${repo_root}/scripts/verify_dev_env.sh" "Splonks supports Apple Silicon macOS development only."
check check_file_contains "macOS package script rejects Intel hosts" "${repo_root}/scripts/package_macos.sh" "package_macos.sh must run on an Apple Silicon Mac because the package is arm64-only."
check check_file_contains "macOS verifier rejects Intel slices" "${repo_root}/scripts/verify_package_macos.sh" 'should be arm64-only but includes x86_64'
check check_file_not_contains "macOS release docs avoid universal default" "${repo_root}/docs/release_distribution.md" 'ship universal by default'
check check_workflow_contains "Package workflow records desktop release evidence" "./scripts/validate_platform.sh release"
check check_workflow_contains "Package workflow bundles desktop release evidence" "./scripts/bundle_validation_evidence.sh --include-artifacts"
check check_workflow_contains "Package workflow uses Android Play handoff wrapper" "./scripts/android/validate_play_handoff.sh"
check check_workflow_contains "Package workflow uses iOS handoff wrapper" "./scripts/validate_ios_handoff.sh"
check check_workflow_not_contains "Package workflow does not pretend hosted iOS upload is complete" "upload_ios_app_store"
check check_workflow_not_contains "Package workflow does not use hosted iOS upload variable" "SPLONKS_UPLOAD_IOS_APP_STORE"
check check_file_contains "iOS handoff has complete evidence mode" "${repo_root}/scripts/validate_ios_handoff.sh" "SPLONKS_IOS_REQUIRE_COMPLETE"
check check_file_contains "Generated iOS handoff requires complete final evidence" "${repo_root}/scripts/print_validation_handoff.sh" "SPLONKS_IOS_REQUIRE_COMPLETE=1 SPLONKS_IOS_UPLOAD=1"

echo
if [[ "${failures}" -eq 0 ]]; then
    echo "[status] complete evidence present"
else
    echo "[status] ${failures} missing evidence item(s)"
fi

if [[ "${strict}" -eq 1 && "${failures}" -ne 0 ]]; then
    exit 1
fi
