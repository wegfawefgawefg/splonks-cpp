#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"

required_env=(
    SPLONKS_ANDROID_KEYSTORE
    SPLONKS_ANDROID_KEYSTORE_PASSWORD
    SPLONKS_ANDROID_KEY_ALIAS
    SPLONKS_ANDROID_KEY_PASSWORD
)

for name in "${required_env[@]}"; do
    if [[ -z "${!name:-}" ]]; then
        echo "Missing ${name} for signed Android release AAB." >&2
        exit 1
    fi
done

if [[ ! -f "${SPLONKS_ANDROID_KEYSTORE}" ]]; then
    echo "Android keystore not found: ${SPLONKS_ANDROID_KEYSTORE}" >&2
    exit 1
fi

require_sdl3_aar
require_android_ndk
require_java

version_code="${SPLONKS_ANDROID_VERSION_CODE:-1}"
version_name="${SPLONKS_ANDROID_VERSION_NAME:-${SPLONKS_RELEASE_VERSION:-0.1.0}}"
dist_dir="${REPO_ROOT}/dist/splonks-android"
aab_src="${ANDROID_DIR}/app/build/outputs/bundle/release/app-release.aab"
aab_dst="${dist_dir}/splonks-${version_name}-android-release.aab"

cd "${ANDROID_DIR}"
if [[ -x ./gradlew ]]; then
    ./gradlew --no-daemon bundleRelease
else
    require_cmd gradle
    gradle --no-daemon bundleRelease
fi

if [[ ! -f "${aab_src}" ]]; then
    echo "AAB not found after build: ${aab_src}" >&2
    exit 1
fi

rm -rf "${dist_dir}"
mkdir -p "${dist_dir}"
cp "${aab_src}" "${aab_dst}"

if command -v sha256sum >/dev/null 2>&1; then
    sha256="$(sha256sum "${aab_dst}" | awk '{print $1}')"
else
    require_cmd shasum
    sha256="$(shasum -a 256 "${aab_dst}" | awk '{print $1}')"
fi
commit="$(git -C "${REPO_ROOT}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
cat > "${dist_dir}/manifest.txt" <<EOF
name=splonks
platform=android
mode=release
artifact=$(basename "${aab_dst}")
version_code=${version_code}
version_name=${version_name}
git_commit=${commit}
sha256=${sha256}
EOF

echo "[package] ${aab_dst}"
echo "[package] sha256=${sha256}"
