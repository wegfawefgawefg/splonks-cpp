#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
bundle_path="${1:-}"
timestamp="$(date -u +"%Y%m%dT%H%M%SZ")"
import_root="${repo_root}/dist/validation-imports"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
expected_revision="${SPLONKS_VALIDATION_REVISION:-$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)}"
allow_stale="${SPLONKS_IMPORT_ALLOW_STALE:-0}"

usage() {
    cat >&2 <<EOF
Usage: $0 <splonks-validation-*.tar.gz>

Imports a validation evidence bundle produced by
scripts/bundle_validation_evidence.sh. Logs, manifests, checksums, and included
release artifacts are copied into the local dist tree so
scripts/validation_status.sh can audit them.

Environment:
  SPLONKS_RELEASE_VERSION      Expected release version, default: ${version}
  SPLONKS_VALIDATION_REVISION  Expected short git revision, default: ${expected_revision}
  SPLONKS_IMPORT_ALLOW_STALE   Set to 1 to import a bundle for another revision/version
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
if [[ -f "${bundle_dir}/CHECKSUMS.sha256" ]]; then
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "${bundle_dir}" && sha256sum -c CHECKSUMS.sha256)
    else
        (cd "${bundle_dir}" && shasum -a 256 -c CHECKSUMS.sha256)
    fi
fi

manifest_value() {
    local key="$1"
    awk -F= -v key="${key}" '$1 == key {print substr($0, length(key) + 2)}' "${bundle_dir}/BUNDLE_MANIFEST.txt" | tail -n 1
}

bundle_version="$(manifest_value release_version)"
bundle_revision="$(manifest_value git_revision)"
if [[ "${allow_stale}" != "1" ]]; then
    if [[ "${bundle_version}" != "${version}" ]]; then
        echo "Evidence bundle release_version=${bundle_version:-<unset>}, expected ${version}." >&2
        echo "Set SPLONKS_IMPORT_ALLOW_STALE=1 only for diagnostic imports." >&2
        exit 1
    fi
    if [[ "${bundle_revision}" != "${expected_revision}" ]]; then
        echo "Evidence bundle git_revision=${bundle_revision:-<unset>}, expected ${expected_revision}." >&2
        echo "Set SPLONKS_IMPORT_ALLOW_STALE=1 only for diagnostic imports." >&2
        exit 1
    fi
fi

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

copy_all_overwrite_if_exists() {
    local src_dir="$1"
    local dst_dir="$2"
    local pattern="$3"
    local src
    if [[ ! -d "${src_dir}" ]]; then
        return
    fi
    mkdir -p "${dst_dir}"
    while IFS= read -r src; do
        cp -f "${src}" "${dst_dir}/$(basename "${src}")"
    done < <(find "${src_dir}" -maxdepth 1 -type f -name "${pattern}" | sort)
}

copy_file_overwrite_if_exists() {
    local src="$1"
    local dst="$2"
    if [[ -f "${src}" ]]; then
        mkdir -p "$(dirname "${dst}")"
        cp -f "${src}" "${dst}"
    fi
}

copy_all_no_clobber_if_exists "${bundle_dir}/validation" "${repo_root}/dist/validation" "*.log"
copy_all_overwrite_if_exists "${bundle_dir}/release-checksums" "${repo_root}/dist/releases" "*.sha256"
copy_all_overwrite_if_exists "${bundle_dir}/release-checksums" "${repo_root}/dist/releases" "*.txt"
copy_all_overwrite_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.tar.gz"
copy_all_overwrite_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.zip"
copy_all_overwrite_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/releases" "*.ipa"
copy_all_overwrite_if_exists "${bundle_dir}/release-artifacts" "${repo_root}/dist/splonks-android" "*.aab"

copy_file_overwrite_if_exists "${bundle_dir}/manifests/linux-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-linux/PACKAGE_MANIFEST.txt"
copy_file_overwrite_if_exists "${bundle_dir}/manifests/macos-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-macos/PACKAGE_MANIFEST.txt"
copy_file_overwrite_if_exists "${bundle_dir}/manifests/windows-PACKAGE_MANIFEST.txt" "${repo_root}/dist/splonks-windows/PACKAGE_MANIFEST.txt"
copy_file_overwrite_if_exists "${bundle_dir}/manifests/android-manifest.txt" "${repo_root}/dist/splonks-android/manifest.txt"
copy_file_overwrite_if_exists "${bundle_dir}/manifests/ios-manifest.txt" "${repo_root}/dist/splonks-ios/manifest.txt"

mkdir -p "${repo_root}/dist/validation-bundles"
bundle_dst="${repo_root}/dist/validation-bundles/$(basename "${bundle_path}")"
if [[ ! -e "${bundle_dst}" ]]; then
    cp "${bundle_path}" "${bundle_dst}"
fi

echo "[import] imported $(basename "${bundle_path}")"
echo "[import] release_version=${bundle_version:-<unset>} git_revision=${bundle_revision:-<unset>}"
echo "[import] run: ./scripts/validation_status.sh"
