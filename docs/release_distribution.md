# Release Distribution

Splonks release artifacts are manual or tag-driven. Normal branch pushes should
not make developers wait on hosted package builds.

Gubsy is not a separate Splonks game release artifact. The current Splonks
release path does not require a separate Gubsy checkout; if future game tooling
uses Gubsy source, it should be pulled into the Splonks build path and never
shipped or packaged as its own consumer dependency for players.

Use [platform_validation.md](platform_validation.md) as the release proof
checklist. A platform is not considered done until its artifact, checksum, and
launch/install/signing/upload evidence are recorded there.

For handoff validation on a real platform, run
`SPLONKS_RELEASE_VERSION=<version> ./scripts/validate_platform.sh release`.
The script writes a timestamped evidence log under `dist/validation/`.
Desktop archive verification extracts the release artifact into a temporary
directory and runs the packaged smoke path from that extracted copy on the
matching host platform.
After validation, run `./scripts/bundle_validation_evidence.sh <label>` to
collect logs, package manifests, and release checksums into a single archive
under `dist/validation-bundles/`.
Use `--include-artifacts` for release handoff bundles that should carry the
actual distributable archives, Android AAB, or iOS IPA.
Use `./scripts/release_credentials_preflight.sh <target>` before credentialed
release paths to catch missing signing/upload tools, secrets, and upload
artifacts early. Store upload preflights also check the already-built AAB or
IPA plus its manifest, checksum, and bundled release contents before invoking
the store upload path.

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

Build and verify on an Apple Silicon Mac:

```bash
./scripts/package_macos.sh
./scripts/verify_package_macos.sh
```

Output:

```text
dist/splonks-macos/Splonks.app
```

Current status: package script exists. It creates an app bundle, copies
assets/data, bundles dylibs, rewrites local dylib references, builds the app
binary as arm64-only for Apple Silicon Macs, verifies that slice set with
`lipo`, and ad-hoc signs locally. Real outside-Mac-distribution still needs
Developer ID signing, notarization, and stapling validation.

macOS policy: ship and validate Apple Silicon only. The setup, developer
verification, and package scripts reject non-arm64 Macs; the release preset
sets `CMAKE_OSX_ARCHITECTURES=arm64`; the package verifier rejects `x86_64`
slices; and the distributable archive is named
`splonks-<version>-macos-arm64.zip`. This avoids producing a universal binary,
keeps bundled native code smaller, and removes Intel Macs from the release
validation matrix. Intel Macs are intentionally out of scope for Splonks macOS
developer and release support unless we later see a real user need to add a
separate compatibility target.

Developer ID signing and notarization path:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_MACOS_BUNDLE_ID=dev.splonks.game
export SPLONKS_MACOS_SIGN_IDENTITY="Developer ID Application: Name (TEAMID)"
export SPLONKS_NOTARYTOOL_PROFILE=splonks-notary
./scripts/validate_platform.sh macos-notarized
```

Alternatively, use Apple ID credentials instead of a stored notarytool profile:

```bash
export APPLE_ID=dev@example.com
export APPLE_TEAM_ID=TEAMID
export APPLE_APP_SPECIFIC_PASSWORD=...
```

Output:

```text
dist/releases/splonks-0.1.0-macos-arm64.zip
dist/releases/splonks-0.1.0-macos-arm64.zip.sha256
```

`macos-notarized` builds the package, verifies the local app bundle, signs with
the Developer ID identity, submits to notarytool, staples the ticket, writes the
release zip/checksum, extracts the zip, applies a quarantine attribute, runs
Gatekeeper assessment, launches the quarantined extracted app smoke, and
verifies the release archive.

Optional macOS package metadata:

```text
SPLONKS_MACOS_BUNDLE_ID=dev.splonks.game
SPLONKS_MACOS_BUNDLE_NAME=Splonks
SPLONKS_RELEASE_VERSION=0.1.0
```

## Windows

Build, verify, and archive from MSYS2 UCRT64 on Windows:

```bash
test "${MSYSTEM:-}" = UCRT64
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
DLLs, manifest, and `run-splonks.bat`. The package and archive verifiers launch
through `run-splonks.bat` from the packaged/extracted directory. It still needs
validation on a real Windows machine after the desktop onboarding cleanup.

## Android

Debug/dev APK:

```bash
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_apk.sh
```

Signed release AAB:

```bash
eval "$(./scripts/android/create_validation_keystore.sh)"
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
./scripts/android/setup_sdk.sh
./scripts/android/fetch_sdl3_aar.sh
./scripts/android/build_release_aab.sh
./scripts/android/verify_release_aab.sh
```

For real Play distribution, replace the throwaway validation keystore with the
real upload keystore:

```bash
export SPLONKS_ANDROID_KEYSTORE=/absolute/path/to/upload-keystore.jks
export SPLONKS_ANDROID_KEYSTORE_PASSWORD=...
export SPLONKS_ANDROID_KEYSTORE_TYPE=jks
export SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload
export SPLONKS_ANDROID_KEY_ALIAS=...
export SPLONKS_ANDROID_KEY_PASSWORD=...
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_ANDROID_VERSION_CODE=1
./scripts/android/validate_play_handoff.sh
```

Output:

```text
dist/splonks-android/splonks-<version>-android-release.aab
dist/splonks-android/manifest.txt
```

Current status: debug APK build, x86_64 emulator runtime smoke, signed arm64
release AAB build, and AAB artifact verification are validated locally. The
release AAB includes `libmain.so`, SDL3 runtime libraries, `assets/`, and
`data/`, and writes a manifest with version, commit, and SHA-256. The AAB
verifier checks the manifest version against the release version before upload.
The manifest also records `keystore_purpose`; local throwaway builds are stamped
`validation`, while Play-ready AABs must be rebuilt with the real upload key and
`SPLONKS_ANDROID_KEYSTORE_PURPOSE=upload`. The throwaway helper defaults to
`dist/local/android-validation.jks`; do not use that key for Play. Final
distribution still needs the real upload key and Play Console upload validation. See
[android_play_release.md](android_play_release.md) for the Play Console internal
testing and production handoff.

Play internal testing upload helper:

```bash
export SPLONKS_PLAY_SERVICE_ACCOUNT_JSON=/absolute/path/to/google-play-service-account.json
export SPLONKS_ANDROID_PACKAGE_NAME=dev.splonks.game
export SPLONKS_PLAY_TRACK=internal
export SPLONKS_PLAY_RELEASE_STATUS=draft
SPLONKS_PLAY_UPLOAD=1 ./scripts/android/validate_play_handoff.sh
```

The handoff wrapper creates current `android-release` validation evidence from
the real upload key, then runs the Play upload helper when
`SPLONKS_PLAY_UPLOAD=1` is set.
Default upload-key AAB validation writes
`splonks-validation-android-play-partial-*.tar.gz`; only a non-validate-only
Play upload writes the final `splonks-validation-android-play-*.tar.gz` bundle
that should be imported with `SPLONKS_IMPORT_EXPECT_TARGET=android-play`.
Set `SPLONKS_PLAY_RELEASE_STATUS=completed` only when intentionally rolling out
to internal testers.

## iOS

External iOS validators should use the handoff wrapper after clone/checkout:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_IOS_BUNDLE_ID=dev.splonks.game
./scripts/validate_ios_handoff.sh
```

The wrapper always validates the simulator path and bundles evidence. It runs
signed archive/export when `SPLONKS_IOS_DEVELOPMENT_TEAM` is set, physical
device install/launch when `SPLONKS_IOS_DEVICE_ID` is set, and App Store
Connect/TestFlight upload only when `SPLONKS_IOS_UPLOAD=1` is set.

Use the default wrapper mode for simulator-only or partial iOS validation. For
final release evidence, run the handoff with
`SPLONKS_IOS_REQUIRE_COMPLETE=1 SPLONKS_IOS_UPLOAD=1` after setting the signing,
device, and App Store Connect credentials; that mode fails before bundling if
the returned evidence would not satisfy the complete `ios` target. Default
partial runs write `splonks-validation-ios-partial-*.tar.gz`; only complete
mode writes the importable final `splonks-validation-ios-*.tar.gz` bundle.

Simulator build on macOS/Xcode:

```bash
xcode-select --install
brew --version >/dev/null 2>&1 || /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
brew install cmake
./scripts/ios/build_sim.sh
./scripts/ios/run_sim.sh --check-state-fingerprint-smoke
```

Device archive/export path on macOS/Xcode:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_IOS_DEVELOPMENT_TEAM=TEAMID
export SPLONKS_IOS_BUNDLE_ID=dev.splonks.game
export SPLONKS_IOS_CODE_SIGN_IDENTITY="Apple Distribution"
export SPLONKS_IOS_EXPORT_METHOD=app-store-connect
./scripts/ios/archive_release.sh
./scripts/ios/verify_release_ipa.sh
```

Physical device install/launch validation after archive/export:

```bash
export SPLONKS_IOS_DEVICE_ID=<device-id-from-xcrun-devicectl-list-devices>
./scripts/ios/install_device.sh
```

App Store Connect/TestFlight upload path:

```bash
export SPLONKS_RELEASE_VERSION=0.1.0
export SPLONKS_APP_STORE_API_KEY=ABC123DEFG
export SPLONKS_APP_STORE_API_ISSUER=00000000-0000-0000-0000-000000000000
export API_PRIVATE_KEYS_DIR=/absolute/path/to/appstoreconnect/private_keys
./scripts/ios/upload_app_store.sh validate-upload
```

Apple's App Store Connect help documents Xcode, Transporter, and `xcrun
altool` as supported upload paths:
<https://developer.apple.com/help/app-store-connect/manage-builds/upload-builds>.
`altool` expects the API key file to be named
`AuthKey_<SPLONKS_APP_STORE_API_KEY>.p8`; it searches `./private_keys`,
`~/private_keys`, `~/.private_keys`, `~/.appstoreconnect/private_keys`, and
`API_PRIVATE_KEYS_DIR`.

Alternatively, use Apple ID credentials with an app-specific password:

```bash
export APPLE_ID=dev@example.com
export APPLE_APP_SPECIFIC_PASSWORD=...
./scripts/ios/upload_app_store.sh validate-upload
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
script now has a runtime smoke mode that requires the state-fingerprint success
line. The archive/export script exists, and the archive path writes a manifest
with bundle id, export method, commit, version, and SHA-256. The IPA verifier
checks the exported IPA, release version, current git revision, checksum,
manifest, bundled content, and macOS `Info.plist` version fields before upload.
The device install helper uses `xcrun devicectl` to
install and launch the signed archive app on a connected provisioned device.
The upload helper uses `xcrun altool` to validate and upload the exported IPA
to App Store Connect/TestFlight. These paths still need macOS/Xcode validation
with an Apple Developer team.

- Final App Store distribution checklist.
- Validation of upload through Xcode Organizer, Transporter, or
  `scripts/ios/upload_app_store.sh`.
- Confirmation of provisioning profile and entitlements for the final bundle ID.

Optional iOS archive/export settings:

```text
SPLONKS_IOS_SIGNING_STYLE=automatic|manual
SPLONKS_IOS_PROVISIONING_PROFILE=<profile name>
SPLONKS_IOS_VERSION_CODE=1   # maps to CFBundleVersion
SPLONKS_IOS_SIMULATOR_UDID=<simulator udid>
SPLONKS_IOS_IPA_PATH=<explicit ipa path>
API_PRIVATE_KEYS_DIR=<directory containing AuthKey_<key id>.p8>
```

For non-interactive CI signing, store the certificate and provisioning profile
outside the repo and import them before `archive_release.sh`:

```bash
export SPLONKS_IOS_CERTIFICATE_BASE64=<base64 p12>
export SPLONKS_IOS_CERTIFICATE_PASSWORD=<p12 password>
export SPLONKS_IOS_PROVISIONING_PROFILE_BASE64=<base64 mobileprovision>
export SPLONKS_IOS_KEYCHAIN_PASSWORD=<temporary keychain password>
./scripts/ios/import_signing_assets.sh
```

## GitHub Actions

The package workflow is intentionally limited to manual dispatch and `v*` tags.
Do not add package jobs back to normal branch pushes.
macOS package and iOS release jobs use the explicit `macos-15` GitHub-hosted
runner label so hosted release validation matches the Apple Silicon-only
artifact policy.

Manual dispatch accepts a `release_version` input. Version tags use the tag name
with a leading `v` stripped, so `v0.1.0` produces `0.1.0` artifacts.
The signed iOS IPA job is opt-in: set `include_ios` on manual dispatch, or set
the repository variable `SPLONKS_BUILD_IOS_RELEASE=true` for tagged release
runs that should include iOS.
Android Play delivery is also opt-in. Set `upload_android_play` on manual
dispatch, or set the repository variable `SPLONKS_UPLOAD_ANDROID_PLAY=true` for
tagged release runs that should submit to Google Play. iOS App
Store/TestFlight delivery is not run from the GitHub-hosted macOS job because
complete iOS release evidence requires a provisioned physical device; use the
external Mac+device handoff for that final step.

Workflow artifacts:

- Linux: `splonks-<version>-linux-x86_64.tar.gz` plus `.sha256`,
  `dist/splonks-linux/PACKAGE_MANIFEST.txt`, validation logs, and an importable
  evidence bundle.
- macOS: `splonks-<version>-macos-arm64.zip` plus `.sha256`,
  `dist/splonks-macos/PACKAGE_MANIFEST.txt`, validation logs, and an importable
  evidence bundle.
- Windows: `splonks-<version>-windows-x86_64.zip` plus `.sha256`,
  `dist/splonks-windows/PACKAGE_MANIFEST.txt`, validation logs, and an
  importable evidence bundle.
- Android: debug APK for remote sanity checks on every manual/tag run.
- Android release: signed `splonks-<version>-android-release.aab` plus
  `manifest.txt`, validation logs, and validation bundles when Android signing
  secrets are configured.
- Android Play: optional upload through `scripts/android/validate_play_handoff.sh`
  when explicitly requested and Google Play credentials are configured.
- iOS release: signed `splonks-<version>-ios.ipa`, `.sha256`, and
  `manifest.txt`, validation logs, and validation bundles when the opt-in iOS
  job and Apple signing secrets are configured.
- iOS App Store/TestFlight: external Mac+device validation/upload through
  `scripts/validate_ios_handoff.sh` with
  `SPLONKS_IOS_REQUIRE_COMPLETE=1 SPLONKS_IOS_UPLOAD=1` after signing,
  physical device, and App Store Connect credentials are configured.

Required Android release secrets:

```text
SPLONKS_ANDROID_KEYSTORE_BASE64
SPLONKS_ANDROID_KEYSTORE_PASSWORD
SPLONKS_ANDROID_KEYSTORE_TYPE
SPLONKS_ANDROID_KEY_ALIAS
SPLONKS_ANDROID_KEY_PASSWORD
SPLONKS_PLAY_SERVICE_ACCOUNT_JSON_BASE64
```

`SPLONKS_ANDROID_KEYSTORE_BASE64` is the base64-encoded upload keystore file.
`SPLONKS_PLAY_SERVICE_ACCOUNT_JSON_BASE64` is the base64-encoded Google Play
service account JSON key used only for the explicit Play upload path.
Keep the real upload keystore outside the repo.

Required iOS release variables/secrets:

```text
SPLONKS_IOS_DEVELOPMENT_TEAM       repository variable
SPLONKS_IOS_BUNDLE_ID              repository variable
SPLONKS_APP_STORE_API_KEY          repository variable
SPLONKS_APP_STORE_API_ISSUER       repository variable
SPLONKS_IOS_CERTIFICATE_BASE64     secret
SPLONKS_IOS_CERTIFICATE_PASSWORD   secret
SPLONKS_IOS_PROVISIONING_PROFILE_BASE64
                                   secret
SPLONKS_IOS_KEYCHAIN_PASSWORD      secret
SPLONKS_APP_STORE_API_PRIVATE_KEY_BASE64
                                   secret
```

`SPLONKS_IOS_CERTIFICATE_BASE64` is the base64-encoded Apple Distribution
`.p12`; `SPLONKS_IOS_PROVISIONING_PROFILE_BASE64` is the base64-encoded
`.mobileprovision` for the final bundle ID.
`SPLONKS_APP_STORE_API_PRIVATE_KEY_BASE64` is the base64-encoded
`AuthKey_<key id>.p8` App Store Connect API private key used only for the
explicit App Store/TestFlight upload path.
