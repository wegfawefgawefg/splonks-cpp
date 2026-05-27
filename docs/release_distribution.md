# Release Distribution

Splonks release artifacts are manual or tag-driven. Normal branch pushes should
not make developers wait on hosted package builds.

Gubsy is included as source/tooling through the Splonks build path. Do not ship
or package Gubsy separately for a Splonks game release.

## Linux

Build, verify, and archive:

```bash
./scripts/package_linux.sh
./scripts/verify_package_linux.sh
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/archive_release.sh linux
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/verify_release_archive.sh linux
```

Output:

```text
dist/splonks-linux/
dist/releases/splonks-0.1.0-linux-x86_64.tar.gz
dist/releases/splonks-0.1.0-linux-x86_64.tar.gz.sha256
```

Current status: verified locally on Linux. The package includes the game
binary, `assets/`, `data/`, bundled SDL runtime libraries, a package manifest,
and `run-splonks.sh`.

## macOS

Build and verify on macOS:

```bash
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
```

Output:

```text
dist/splonks-macos/Splonks.app
```

Current status: package script exists. It creates an app bundle, copies
assets/data, bundles dylibs, rewrites local dylib references, and ad-hoc signs
locally. Real outside-Mac-distribution still needs Developer ID signing,
notarization, and stapling validation.

Developer ID signing and notarization path:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
export SPLONKS_NOTARYTOOL_PROFILE=splonks-notary
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
./scripts/macos/notarize_app.sh
./scripts/verify_release_archive.sh macos
```

Alternatively, use Apple ID credentials instead of a stored notarytool profile:

```bash
export APPLE_ID=dev@example.com
export APPLE_TEAM_ID=TEAMID
export APPLE_APP_SPECIFIC_PASSWORD=...
```

Output:

```text
dist/releases/splonks-0.1.0-macos-universal.zip
dist/releases/splonks-0.1.0-macos-universal.zip.sha256
```

## Windows

Build, verify, and archive from MSYS2 UCRT64 on Windows:

```bash
./scripts/package_windows.sh
./scripts/verify_package_windows.sh
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/archive_release.sh windows
SPLONKS_RELEASE_VERSION=0.1.0 ./scripts/verify_release_archive.sh windows
```

Output:

```text
dist/splonks-windows/
dist/releases/splonks-0.1.0-windows-x86_64.zip
dist/releases/splonks-0.1.0-windows-x86_64.zip.sha256
```

Current status: package script exists. It copies the executable, assets/data,
DLLs, manifest, and `run-splonks.bat`. It still needs validation on a real
Windows machine after the desktop onboarding cleanup.

## Android

Debug/dev APK:

```bash
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_apk.sh
```

Signed release AAB:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_ANDROID_VERSION_CODE=1
export SPLONKS_ANDROID_VERSION_NAME=0.1.0
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
```

Output:

```text
dist/splonks-android/splonks-<version>-android-release.aab
dist/splonks-android/manifest.txt
```

Current status: debug APK build, x86_64 emulator runtime smoke, and signed
arm64 release AAB build are validated locally. The release AAB includes
`libmain.so`, SDL3 runtime libraries, `assets/`, and `data/`, and writes a
manifest with version, commit, and SHA-256. Final distribution still needs the
real upload key and Play Console upload validation.

## iOS

Simulator build on macOS/Xcode:

```bash
./scripts/ios/build_sim.sh
./scripts/ios/run_sim.sh
```

Device archive/export path on macOS/Xcode:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_IOS_DEVELOPMENT_TEAM=TEAMID
export SPLONKS_IOS_BUNDLE_ID=dev.splonks.game
export SPLONKS_IOS_CODE_SIGN_IDENTITY="Apple Distribution"
export SPLONKS_IOS_EXPORT_METHOD=app-store-connect
./scripts/ios/archive_release.sh
```

Output:

```text
dist/splonks-ios/Splonks.xcarchive
dist/splonks-ios/export/
dist/splonks-ios/manifest.txt
dist/releases/splonks-0.1.0-ios.ipa
dist/releases/splonks-0.1.0-ios.ipa.sha256
```

Current status: `ios-sim` and `ios-device` CMake/Xcode scaffolds exist and copy
`assets/` and `data/` into the app bundle. The simulator build/install/launch
script and archive/export script exist, and the archive path writes a manifest
with bundle id, export method, commit, version, and SHA-256. These paths still
need macOS/Xcode validation with an Apple Developer team.

- Final App Store distribution checklist.
- Validation of upload through Xcode Organizer, Transporter, or `xcrun altool`.
- Confirmation of provisioning profile and entitlements for the final bundle ID.

Optional iOS archive/export settings:

```text
SPLONKS_IOS_SIGNING_STYLE=automatic|manual
SPLONKS_IOS_PROVISIONING_PROFILE=<profile name>
SPLONKS_IOS_VERSION_CODE=1
SPLONKS_IOS_SIMULATOR_UDID=<simulator udid>
```

## GitHub Actions

The package workflow is intentionally limited to manual dispatch and `v*` tags.
Do not add package jobs back to normal branch pushes.

Manual dispatch accepts a `release_version` input. Version tags use the tag name
with a leading `v` stripped, so `v0.1.0` produces `0.1.0` artifacts.

Workflow artifacts:

- Linux: `splonks-<version>-linux-x86_64.tar.gz` plus `.sha256`.
- macOS: `splonks-<version>-macos-universal.zip` plus `.sha256`.
- Windows: `splonks-<version>-windows-x86_64.zip` plus `.sha256`.
- Android: debug APK for remote sanity checks on every manual/tag run.
- Android release: signed `splonks-<version>-android-release.aab` plus
  `manifest.txt` when Android signing secrets are configured.

Required Android release secrets:

```text
SPLONKS_ANDROID_KEYSTORE_BASE64
SPLONKS_ANDROID_KEYSTORE_PASSWORD
SPLONKS_ANDROID_KEY_ALIAS
SPLONKS_ANDROID_KEY_PASSWORD
```

`SPLONKS_ANDROID_KEYSTORE_BASE64` is the base64-encoded upload keystore file.
Keep the real upload keystore outside the repo.
