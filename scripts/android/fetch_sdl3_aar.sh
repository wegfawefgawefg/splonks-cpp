#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

require_cmd curl
require_cmd unzip

url="https://github.com/libsdl-org/SDL/releases/download/release-${SDL3_ANDROID_VERSION}/SDL3-devel-${SDL3_ANDROID_VERSION}-android.zip"
tmp_dir="$(mktemp -d)"
zip_path="${tmp_dir}/SDL3-devel-${SDL3_ANDROID_VERSION}-android.zip"
libs_dir="${ANDROID_DIR}/app/libs"
aar_name="SDL3-${SDL3_ANDROID_VERSION}.aar"

cleanup() {
    rm -rf "${tmp_dir}"
}
trap cleanup EXIT

mkdir -p "${libs_dir}"

echo "Downloading ${url}"
curl -L --fail -o "${zip_path}" "${url}"

actual_sha="$(sha256sum "${zip_path}" | awk '{print $1}')"
if [[ "${actual_sha}" != "${SDL3_ANDROID_ZIP_SHA256}" ]]; then
    echo "SDL3 Android archive checksum mismatch" >&2
    echo "expected: ${SDL3_ANDROID_ZIP_SHA256}" >&2
    echo "actual:   ${actual_sha}" >&2
    exit 1
fi

unzip -j -o "${zip_path}" "${aar_name}" -d "${libs_dir}"

echo "[android] installed ${libs_dir}/${aar_name}"
