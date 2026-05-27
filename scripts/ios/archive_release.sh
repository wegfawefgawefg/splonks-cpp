#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "archive_release.sh must run on macOS with Xcode installed" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
build_dir="${repo_root}/build/ios-device"
archive_dir="${repo_root}/dist/splonks-ios"
release_dir="${repo_root}/dist/releases"
archive_path="${archive_dir}/Splonks.xcarchive"
export_path="${archive_dir}/export"
export_options="${archive_dir}/ExportOptions.plist"
ipa_path="${release_dir}/splonks-${version}-ios.ipa"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing command: $1" >&2
        exit 1
    fi
}

require_cmd cmake
require_cmd xcodebuild

normalize_signing_style() {
    local style_lower
    style_lower="$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')"
    case "${style_lower}" in
        automatic) printf 'Automatic' ;;
        manual) printf 'Manual' ;;
        *)
            echo "SPLONKS_IOS_SIGNING_STYLE must be automatic or manual, got: $1" >&2
            exit 1
            ;;
    esac
}

if [[ -z "${SPLONKS_IOS_DEVELOPMENT_TEAM:-}" ]]; then
    echo "Missing SPLONKS_IOS_DEVELOPMENT_TEAM for device signing." >&2
    exit 1
fi

export SPLONKS_IOS_CODE_SIGN_IDENTITY="${SPLONKS_IOS_CODE_SIGN_IDENTITY:-Apple Distribution}"
export SPLONKS_IOS_EXPORT_METHOD="${SPLONKS_IOS_EXPORT_METHOD:-app-store-connect}"
export SPLONKS_IOS_BUNDLE_ID="${SPLONKS_IOS_BUNDLE_ID:-dev.splonks.game}"
export SPLONKS_IOS_SIGNING_STYLE="$(normalize_signing_style "${SPLONKS_IOS_SIGNING_STYLE:-automatic}")"
SPLONKS_IOS_EXPORT_SIGNING_STYLE="$(printf '%s' "${SPLONKS_IOS_SIGNING_STYLE}" | tr '[:upper:]' '[:lower:]')"
export SPLONKS_IOS_VERSION_CODE="${SPLONKS_IOS_VERSION_CODE:-1}"

cd "${repo_root}"
cmake --preset ios-device \
    -DSPLONKS_BUNDLE_VERSION="${version}" \
    -DSPLONKS_IOS_VERSION_CODE="${SPLONKS_IOS_VERSION_CODE}" \
    -DSPLONKS_IOS_BUNDLE_ID="${SPLONKS_IOS_BUNDLE_ID}" \
    -DSPLONKS_IOS_DEVELOPMENT_TEAM="${SPLONKS_IOS_DEVELOPMENT_TEAM}" \
    -DSPLONKS_IOS_CODE_SIGN_IDENTITY="${SPLONKS_IOS_CODE_SIGN_IDENTITY}" \
    -DSPLONKS_IOS_CODE_SIGN_STYLE="${SPLONKS_IOS_SIGNING_STYLE}"

rm -rf "${archive_path}" "${export_path}"
mkdir -p "${archive_dir}" "${release_dir}"

xcodebuild \
    -project "${build_dir}/splonks_cpp.xcodeproj" \
    -scheme splonks-cpp \
    -configuration Release \
    -sdk iphoneos \
    -archivePath "${archive_path}" \
    DEVELOPMENT_TEAM="${SPLONKS_IOS_DEVELOPMENT_TEAM}" \
    CODE_SIGN_IDENTITY="${SPLONKS_IOS_CODE_SIGN_IDENTITY}" \
    archive

cat > "${export_options}" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>method</key>
  <string>${SPLONKS_IOS_EXPORT_METHOD}</string>
  <key>teamID</key>
  <string>${SPLONKS_IOS_DEVELOPMENT_TEAM}</string>
  <key>signingStyle</key>
  <string>${SPLONKS_IOS_EXPORT_SIGNING_STYLE}</string>
EOF

if [[ -n "${SPLONKS_IOS_PROVISIONING_PROFILE:-}" ]]; then
    cat >> "${export_options}" <<EOF
  <key>provisioningProfiles</key>
  <dict>
    <key>${SPLONKS_IOS_BUNDLE_ID}</key>
    <string>${SPLONKS_IOS_PROVISIONING_PROFILE}</string>
  </dict>
EOF
fi

cat >> "${export_options}" <<EOF
  <key>destination</key>
  <string>export</string>
  <key>stripSwiftSymbols</key>
  <true/>
  <key>uploadSymbols</key>
  <true/>
</dict>
</plist>
EOF

xcodebuild -exportArchive \
    -archivePath "${archive_path}" \
    -exportPath "${export_path}" \
    -exportOptionsPlist "${export_options}"

found_ipa="$(find "${export_path}" -maxdepth 1 -type f -name "*.ipa" | head -n 1)"
if [[ -z "${found_ipa}" ]]; then
    echo "No IPA found in ${export_path}" >&2
    exit 1
fi

cp "${found_ipa}" "${ipa_path}"
sha256=""
if command -v shasum >/dev/null 2>&1; then
    (cd "${release_dir}" && shasum -a 256 "$(basename "${ipa_path}")" > "$(basename "${ipa_path}").sha256")
    sha256="$(awk '{print $1}' "${ipa_path}.sha256")"
fi

commit="$(git -C "${repo_root}" rev-parse --short=12 HEAD 2>/dev/null || echo unknown)"
cat > "${archive_dir}/manifest.txt" <<EOF
name=splonks
platform=ios
mode=release
artifact=$(basename "${ipa_path}")
version_code=${SPLONKS_IOS_VERSION_CODE}
version_name=${version}
git_commit=${commit}
bundle_id=${SPLONKS_IOS_BUNDLE_ID}
export_method=${SPLONKS_IOS_EXPORT_METHOD}
signing_style=${SPLONKS_IOS_SIGNING_STYLE}
export_signing_style=${SPLONKS_IOS_EXPORT_SIGNING_STYLE}
sha256=${sha256}
EOF

env SPLONKS_IOS_IPA_PATH="${ipa_path}" "${repo_root}/scripts/ios/verify_release_ipa.sh"

echo "[ios] ${archive_path}"
echo "[ios] ${archive_dir}/manifest.txt"
echo "[ios] ${ipa_path}"
echo "[ios] ${ipa_path}.sha256"
