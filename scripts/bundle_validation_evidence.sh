#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
label=""
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
validation_dir="${repo_root}/dist/validation"
bundle_root="${repo_root}/dist/validation-bundles"
include_artifacts=0
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
expected_revision="$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
allow_stale="${SPLONKS_BUNDLE_ALLOW_STALE:-0}"

usage() {
    cat >&2 <<EOF
Usage: $0 [--include-artifacts] [label]

Bundles validation logs, package manifests, and release checksums into a
timestamped archive that can be sent back after platform validation.
Use --include-artifacts for final release handoff bundles that should also
carry the distributable archives or store bundles.

Examples:
  $0 macos-dev-release
  $0 --include-artifacts windows-release

Environment:
  SPLONKS_RELEASE_VERSION   Release version this evidence is for, default: ${version}
  SPLONKS_BUNDLE_ALLOW_STALE
                            Set to 1 to include logs from other revisions
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --include-artifacts)
            include_artifacts=1
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            if [[ -n "${label}" ]]; then
                usage
                exit 1
            fi
            label="$1"
            ;;
    esac
    shift
done

if [[ -z "${label}" ]]; then
    case "${OS:-}:$(uname -s)" in
        Windows_NT:*|*:MINGW*|*:MSYS*|*:CYGWIN*) label="windows" ;;
        *:Darwin) label="macos" ;;
        *:Linux) label="linux" ;;
        *) label="unknown" ;;
    esac
fi

safe_label="$(printf '%s' "${label}" | tr -c 'A-Za-z0-9._-' '-')"
stage_dir="${bundle_root}/splonks-validation-${safe_label}-${timestamp}"
archive_path="${stage_dir}.tar.gz"

mkdir -p "${stage_dir}"

copy_dir_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -d "${src}" ]]; then
        mkdir -p "$(dirname "${dst}")"
        cp -a "${src}" "${dst}"
    fi
}

copy_file_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -f "${src}" ]]; then
        mkdir -p "$(dirname "${dst}")"
        cp "${src}" "${dst}"
    fi
}

manifest_value() {
    local path="$1"
    local key="$2"
    awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${path}" | tail -n 1
}

skip_stale_manifest() {
    local src="$1"
    local reason="$2"
    printf '[bundle] skipping stale manifest %s: %s\n' "${src#${repo_root}/}" "${reason}" >&2
}

copy_package_manifest_if_current() {
    local src="$1"
    local dst="$2"
    local platform="$3"
    local actual_platform
    local actual_version
    local actual_revision
    if [[ ! -f "${src}" ]]; then
        return 1
    fi
    if [[ "${allow_stale}" == "1" ]]; then
        copy_file_if_exists "${src}" "${dst}"
        return 0
    fi
    actual_platform="$(manifest_value "${src}" platform)"
    actual_version="$(manifest_value "${src}" release_version)"
    actual_revision="$(manifest_value "${src}" git_revision)"
    if [[ "${actual_platform}" != "${platform}" ]]; then
        skip_stale_manifest "${src}" "platform=${actual_platform:-<unset>}, expected ${platform}"
        return 1
    fi
    if [[ "${actual_version}" != "${version}" ]]; then
        skip_stale_manifest "${src}" "release_version=${actual_version:-<unset>}, expected ${version}"
        return 1
    fi
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        skip_stale_manifest "${src}" "git_revision=${actual_revision:-<unset>}, expected ${expected_revision}"
        return 1
    fi
    copy_file_if_exists "${src}" "${dst}"
    return 0
}

copy_store_manifest_if_current() {
    local src="$1"
    local dst="$2"
    local platform="$3"
    local actual_platform
    local actual_version
    local actual_revision
    if [[ ! -f "${src}" ]]; then
        return 1
    fi
    if [[ "${allow_stale}" == "1" ]]; then
        copy_file_if_exists "${src}" "${dst}"
        return 0
    fi
    actual_platform="$(manifest_value "${src}" platform)"
    actual_version="$(manifest_value "${src}" version_name)"
    actual_revision="$(manifest_value "${src}" git_commit)"
    if [[ "${actual_platform}" != "${platform}" ]]; then
        skip_stale_manifest "${src}" "platform=${actual_platform:-<unset>}, expected ${platform}"
        return 1
    fi
    if [[ "${actual_version}" != "${version}" ]]; then
        skip_stale_manifest "${src}" "version_name=${actual_version:-<unset>}, expected ${version}"
        return 1
    fi
    if [[ "${actual_revision}" != "${expected_revision}" ]]; then
        skip_stale_manifest "${src}" "git_commit=${actual_revision:-<unset>}, expected ${expected_revision}"
        return 1
    fi
    copy_file_if_exists "${src}" "${dst}"
    return 0
}

copy_release_file_if_current() {
    local enabled="$1"
    local src="$2"
    local dst_dir="$3"
    if [[ "${enabled}" == "1" && -f "${src}" ]]; then
        mkdir -p "${dst_dir}"
        cp "${src}" "${dst_dir}/$(basename "${src}")"
    fi
}

has_validation_logs() {
    [[ -d "${validation_dir}" ]] || return 1
    [[ -n "$(find "${validation_dir}" -maxdepth 1 -type f -name "*.log" -print -quit)" ]]
}

write_bundle_checksums() {
    local summary_path="${stage_dir}/CHECKSUMS.sha256"
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "${stage_dir}" && find . -type f ! -name "CHECKSUMS.sha256" -print0 \
            | sort -z \
            | xargs -0 sha256sum) > "${summary_path}"
    else
        (cd "${stage_dir}" && find . -type f ! -name "CHECKSUMS.sha256" -print0 \
            | sort -z \
            | xargs -0 shasum -a 256) > "${summary_path}"
    fi
}

log_matches_bundle() {
    local path="$1"
    if [[ "${allow_stale}" == "1" ]]; then
        return 0
    fi
    if ! grep -Fxq "git_revision=${expected_revision}" "${path}"; then
        return 1
    fi
    if grep -q '^release_version=' "${path}" && ! grep -Fxq "release_version=${version}" "${path}"; then
        return 1
    fi
    return 0
}

copied_logs=0
if has_validation_logs; then
    mkdir -p "${stage_dir}/validation"
    while IFS= read -r log_path; do
        if log_matches_bundle "${log_path}"; then
            cp "${log_path}" "${stage_dir}/validation/"
            copied_logs=$((copied_logs + 1))
        fi
    done < <(find "${validation_dir}" -maxdepth 1 -type f -name "*.log" | sort)
else
    echo "No validation logs found under ${validation_dir}" >&2
    echo "Run ./scripts/validate_platform.sh <scope> before bundling evidence." >&2
    exit 1
fi

if [[ "${copied_logs}" -eq 0 ]]; then
    echo "No validation logs under ${validation_dir} matched git_revision=${expected_revision} release_version=${version}." >&2
    echo "Run ./scripts/validate_platform.sh <scope> for this revision, or set SPLONKS_BUNDLE_ALLOW_STALE=1 for diagnostic bundles." >&2
    exit 1
fi

copied_linux_manifest=0
copied_macos_manifest=0
copied_windows_manifest=0
copied_android_manifest=0
copied_ios_manifest=0
if copy_package_manifest_if_current "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/linux-PACKAGE_MANIFEST.txt" linux; then
    copied_linux_manifest=1
fi
if copy_package_manifest_if_current "${repo_root}/dist/splonks-macos/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/macos-PACKAGE_MANIFEST.txt" macos; then
    copied_macos_manifest=1
fi
if copy_package_manifest_if_current "${repo_root}/dist/splonks-windows/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/windows-PACKAGE_MANIFEST.txt" windows; then
    copied_windows_manifest=1
fi
if copy_store_manifest_if_current "${repo_root}/dist/splonks-android/manifest.txt" "${stage_dir}/manifests/android-manifest.txt" android; then
    copied_android_manifest=1
fi
if copy_store_manifest_if_current "${repo_root}/dist/splonks-ios/manifest.txt" "${stage_dir}/manifests/ios-manifest.txt" ios; then
    copied_ios_manifest=1
fi

if [[ -d "${repo_root}/dist/releases" ]]; then
    copy_release_file_if_current "${copied_linux_manifest}" "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz.sha256" "${stage_dir}/release-checksums"
    copy_release_file_if_current "${copied_macos_manifest}" "${repo_root}/dist/releases/splonks-${version}-macos-arm64.zip.sha256" "${stage_dir}/release-checksums"
    copy_release_file_if_current "${copied_windows_manifest}" "${repo_root}/dist/releases/splonks-${version}-windows-x86_64.zip.sha256" "${stage_dir}/release-checksums"
    copy_release_file_if_current "${copied_ios_manifest}" "${repo_root}/dist/releases/splonks-${version}-ios.ipa.sha256" "${stage_dir}/release-checksums"
    copy_release_file_if_current "${copied_ios_manifest}" "${repo_root}/dist/releases/splonks-${version}-ios-manifest.txt" "${stage_dir}/release-checksums"
    if [[ "${include_artifacts}" -eq 1 ]]; then
        copy_release_file_if_current "${copied_linux_manifest}" "${repo_root}/dist/releases/splonks-${version}-linux-x86_64.tar.gz" "${stage_dir}/release-artifacts"
        copy_release_file_if_current "${copied_macos_manifest}" "${repo_root}/dist/releases/splonks-${version}-macos-arm64.zip" "${stage_dir}/release-artifacts"
        copy_release_file_if_current "${copied_windows_manifest}" "${repo_root}/dist/releases/splonks-${version}-windows-x86_64.zip" "${stage_dir}/release-artifacts"
        copy_release_file_if_current "${copied_ios_manifest}" "${repo_root}/dist/releases/splonks-${version}-ios.ipa" "${stage_dir}/release-artifacts"
    fi
fi

if [[ "${include_artifacts}" -eq 1 ]]; then
    copy_release_file_if_current "${copied_android_manifest}" "${repo_root}/dist/splonks-android/splonks-${version}-android-release.aab" "${stage_dir}/release-artifacts"
fi

: > "${stage_dir}/CHECKSUMS.sha256"
{
    echo "label=${label}"
    echo "timestamp_utc=${timestamp}"
    echo "release_version=${version}"
    echo "git_revision=${expected_revision}"
    echo "git_branch=$(git -C "${repo_root}" branch --show-current 2>/dev/null || echo unknown)"
    echo "include_artifacts=${include_artifacts}"
    echo "allow_stale=${allow_stale}"
    echo "uname=$(uname -a)"
    echo "checksum_summary=CHECKSUMS.sha256"
    echo
    echo "[included]"
    find "${stage_dir}" -type f | sort | sed "s#^${stage_dir}/##"
} > "${stage_dir}/BUNDLE_MANIFEST.txt"

write_bundle_checksums

mkdir -p "${bundle_root}"
tar -C "${bundle_root}" -czf "${archive_path}" "$(basename "${stage_dir}")"

echo "[bundle] ${archive_path}"
