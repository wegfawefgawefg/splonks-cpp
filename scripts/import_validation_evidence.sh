#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
bundle_path="${1:-}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
import_root="${repo_root}/dist/validation-imports"

usage() {
    cat >&2 <<EOF
Usage: $0 <splonks-validation-*.tar.gz>

Imports a validation evidence bundle produced by
scripts/bundle_validation_evidence.sh. Logs, manifests, checksums, and included
release artifacts are copied into the local dist tree so
scripts/validation_status.sh can audit them.
EOF
}

case "${bundle_path}" in
    ""|-h|--help|help)
        usage
        if [[ -z "${bundle_path}" || "${bundle_path}" == "" ]]; then
            exit 1
        fi
        exit 0
        ;;
esac

if [[ ! -f "${bundle_path}" ]]; then
    echo "Missing evidence bundle: ${bundle_path}" >&2
    exit 1
fi

case "${bundle_path}" in
    *.tar.gz) ;;
    *)
        echo "Evidence bundle must be a .tar.gz file: ${bundle_path}" >&2
        exit 1
        ;;
esac

mkdir -p "${import_root}"
work_dir="$(mktemp -d "${import_root}/import-${timestamp}-XXXXXX")"
trap 'rm -rf "${work_dir}"' EXIT

tar -C "${work_dir}" -xzf "${bundle_path}"
bundle_dir="$(find "${work_dir}" -mindepth 1 -maxdepth 1 -type d | head -n 1)"
if [[ -z "${bundle_dir}" ]]; then
    echo "Evidence bundle did not contain a top-level directory" >&2
    exit 1
fi
if [[ ! -f "${bundle_dir}/BUNDLE_MANIFEST.txt" ]]; then
    echo "Evidence bundle is missing BUNDLE_MANIFEST.txt" >&2
    exit 1
fi

copy_all_if_exists() {
    local src_dir="$1"
    local dst_dir="$2"
    local pattern="$3"
    if [[ -d "${src_dir}" ]] && find "${src_dir}" -maxdepth 1 -type f -name "${pattern}" | grep -q .; then
        mkdir -p "${dst_dir}"
        find "${src_dir}" -maxdepth 1 -type f -name "${pattern}" -exec cp -n {} "${dst_dir}/" \;
    fi
}

copy_all_no_clobber_if_exists() {
    local src_dir="$1"
    local dst_dir="$2"
    local pattern="$3"
    local src
    if [[ ! -d "${src_dir}" ]]; then
        return
    fi
    mkdir -p "${dst_dir}"
    while IFS= read -r src; do
        local dst="${dst_dir}/$(basename "${src}")"
        if [[ ! -e "${dst}" ]]; then
            cp "${src}" "${dst}"
        fi
    done < <(find "${src_dir}" -maxdepth 1 -type f -name "${pattern}" | sort)
}

copy_file_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -f "${src}" && ! -e "${dst}" ]]; then
        mkdir -p "$(dirname "${dst}")"
        cp "${src}" "${dst}"
    fi
}

copy_all_no_clobber_if_exists "${bundle_dir}/validation" "${repo_root}/dist/validation" "*.log"
copy_all_no_clobber_if_exists "${bundle_dir}/release-checksums" "${repo_root}/dist/releases" "*.sha256"
copy_all_no_clobber_if_exists "${bundle_dir}/release-checksums" "${repo_root}/dist/releases" "*.txt"
copy_all_no_clobber_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.tar.gz"
copy_all_no_clobber_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.zip"
copy_all_no_clobber_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.ipa"
copy_all_no_clobber_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/splonks-android" "*.aab"

copy_file_if_exists "${bundle_dir}/manifests/linux-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt"
copy_file_if_exists "${bundle_dir}/manifests/macos-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-macos/PACKAGE_MANIFEST.txt"
copy_file_if_exists "${bundle_dir}/manifests/windows-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-windows/PACKAGE_MANIFEST.txt"
copy_file_if_exists "${bundle_dir}/manifests/android-manifest.txt" "${repo_root}/dist/splonks-android/manifest.txt"
copy_file_if_exists "${bundle_dir}/manifests/ios-manifest.txt" "${repo_root}/dist/splonks-ios/manifest.txt"

mkdir -p "${repo_root}/dist/validation-bundles"
bundle_dst="${repo_root}/dist/validation-bundles/$(basename "${bundle_path}")"
if [[ ! -e "${bundle_dst}" ]]; then
    cp "${bundle_path}" "${bundle_dst}"
fi

echo "[import] imported $(basename "${bundle_path}")"
echo "[import] run: ./scripts/validation_status.sh"
