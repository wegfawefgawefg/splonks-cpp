#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
label=""
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
validation_dir="${repo_root}/dist/validation"
bundle_root="${repo_root}/dist/validation-bundles"
include_artifacts=0
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"

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

if [[ -d "${validation_dir}" ]] && find "${validation_dir}" -maxdepth 1 -type f -name "*.log" | grep -q .; then
    mkdir -p "${stage_dir}/validation"
    find "${validation_dir}" -maxdepth 1 -type f -name "*.log" -exec cp {} "${stage_dir}/validation/" \;
else
    echo "No validation logs found under ${validation_dir}" >&2
    echo "Run ./scripts/validate_platform.sh <scope> before bundling evidence." >&2
    exit 1
fi

copy_file_if_exists "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/linux-PACKAGE_MANIFEST.txt"
copy_file_if_exists "${repo_root}/dist/splonks-macos/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/macos-PACKAGE_MANIFEST.txt"
copy_file_if_exists "${repo_root}/dist/splonks-windows/PACKAGE_MANIFEST.txt" "${stage_dir}/manifests/windows-PACKAGE_MANIFEST.txt"
copy_file_if_exists "${repo_root}/dist/splonks-android/manifest.txt" "${stage_dir}/manifests/android-manifest.txt"
copy_file_if_exists "${repo_root}/dist/splonks-ios/manifest.txt" "${stage_dir}/manifests/ios-manifest.txt"

if [[ -d "${repo_root}/dist/releases" ]]; then
    mkdir -p "${stage_dir}/release-checksums"
    find "${repo_root}/dist/releases" -maxdepth 1 -type f \( -name "*.sha256" -o -name "*.txt" \) \
        -exec cp {} "${stage_dir}/release-checksums/" \;
    if [[ "${include_artifacts}" -eq 1 ]]; then
        mkdir -p "${stage_dir}/release-artifacts"
        find "${repo_root}/dist/releases" -maxdepth 1 -type f \( -name "*.tar.gz" -o -name "*.zip" -o -name "*.ipa" \) \
            -exec cp {} "${stage_dir}/release-artifacts/" \;
    fi
fi

if [[ "${include_artifacts}" -eq 1 ]]; then
    if [[ -d "${repo_root}/dist/splonks-android" ]]; then
        mkdir -p "${stage_dir}/release-artifacts"
        find "${repo_root}/dist/splonks-android" -maxdepth 1 -type f -name "*.aab" \
            -exec cp {} "${stage_dir}/release-artifacts/" \;
    fi
fi

: > "${stage_dir}/CHECKSUMS.sha256"
{
    echo "label=${label}"
    echo "timestamp_utc=${timestamp}"
    echo "release_version=${version}"
    echo "git_revision=$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
    echo "git_branch=$(git -C "${repo_root}" branch --show-current 2>/dev/null || echo unknown)"
    echo "include_artifacts=${include_artifacts}"
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
