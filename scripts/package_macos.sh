#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "package_macos.sh must run on macOS" >&2
    exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
build_dir="${repo_root}/build-package-macos"
dist_dir="${repo_root}/dist/splonks-macos"
source "${repo_root}/scripts/package_runtime_libs.sh"
version="${SPLONKS_RELEASE_VERSION:-0.1.0}"
bundle_id="${SPLONKS_MACOS_BUNDLE_ID:-dev.splonks.game}"
bundle_name="${SPLONKS_MACOS_BUNDLE_NAME:-Splonks}"
app_dir="${dist_dir}/Splonks.app"
contents_dir="${app_dir}/Contents"
macos_dir="${contents_dir}/MacOS"
resources_dir="${contents_dir}/Resources"
frameworks_dir="${contents_dir}/Frameworks"

SPLONKS_PRESET=package-macos "${repo_root}/scripts/build.sh" \
    -DSPLONKS_BUNDLE_VERSION="${version}"

rm -rf "${dist_dir}"
mkdir -p "${macos_dir}" "${resources_dir}" "${frameworks_dir}"

cp "${build_dir}/splonks-cpp" "${macos_dir}/splonks-bin"
cp -a "${repo_root}/assets" "${resources_dir}/assets"
cp -a "${repo_root}/data" "${resources_dir}/data"

cat > "${macos_dir}/Splonks" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
export DYLD_LIBRARY_PATH="${root}/Frameworks${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
cd "${root}/Resources"
exec "${root}/MacOS/splonks-bin" "$@"
EOF
chmod +x "${macos_dir}/Splonks"

xml_escape() {
    printf '%s' "$1" \
        | sed -e 's/&/\&amp;/g' \
              -e 's/</\&lt;/g' \
              -e 's/>/\&gt;/g' \
              -e 's/"/\&quot;/g' \
              -e "s/'/\&apos;/g"
}

cat > "${contents_dir}/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleExecutable</key>
  <string>Splonks</string>
  <key>CFBundleIdentifier</key>
  <string>$(xml_escape "${bundle_id}")</string>
  <key>CFBundleName</key>
  <string>$(xml_escape "${bundle_name}")</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleVersion</key>
  <string>$(xml_escape "${version}")</string>
  <key>CFBundleShortVersionString</key>
  <string>$(xml_escape "${version}")</string>
</dict>
</plist>
EOF

package_copy_macos_runtime_deps "${macos_dir}/splonks-bin" "${frameworks_dir}"
package_copy_runtime_libs_from_tree "${build_dir}" "${frameworks_dir}" ".dylib"
package_write_manifest "splonks" "macos" "release" "${repo_root}" "${dist_dir}"

find "${frameworks_dir}" -type f -name "*.dylib" -exec chmod u+w {} +
while IFS= read -r dylib; do
    install_name_tool -id "@rpath/$(basename "${dylib}")" "${dylib}" 2>/dev/null || true
done < <(find "${frameworks_dir}" -type f -name "*.dylib")
install_name_tool -add_rpath "@executable_path/../Frameworks" "${macos_dir}/splonks-bin" 2>/dev/null || true

rewrite_dylib_refs() {
    local target="$1"
    otool -L "${target}" \
        | awk 'NR > 1 {print $1}' \
        | while read -r dep; do
            local local_dep="${frameworks_dir}/$(basename "${dep}")"
            if [[ -f "${local_dep}" ]]; then
                install_name_tool -change "${dep}" "@rpath/$(basename "${dep}")" "${target}" 2>/dev/null || true
            fi
        done
}

rewrite_dylib_refs "${macos_dir}/splonks-bin"
while IFS= read -r dylib; do
    rewrite_dylib_refs "${dylib}"
done < <(find "${frameworks_dir}" -type f -name "*.dylib")

codesign --force --deep --sign - "${app_dir}" 2>/dev/null || true

echo "[package] ${dist_dir}"
